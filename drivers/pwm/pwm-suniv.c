/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option)any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/time.h>

#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch-suniv/dma.h>
#include <asm/arch-suniv/cpu.h>
#include <asm/arch-suniv/gpio.h>
#include <asm/arch-suniv/clock.h>
#include <asm/arch-suniv/codec.h>
#include <asm/arch-suniv/common.h>

#define MIYOO_VIR_SET_MODE  _IOWR(0x100, 0, unsigned long)
#define MIYOO_VIR_SET_VER   _IOWR(0x101, 0, unsigned long)

#define PWM_CTRL_REG        0x0
#define PWM_CH_PRD_BASE     0x4
#define PWM_CH_PRD_OFFSET   0x4
#define PWM_CH_PRD(ch)      (PWM_CH_PRD_BASE + PWM_CH_PRD_OFFSET * (ch))

#define PWMCH_OFFSET        15
#define PWM_PRESCAL_MASK    GENMASK(3, 0)
#define PWM_PRESCAL_OFF     0
#define PWM_EN              BIT(4)
#define PWM_ACT_STATE       BIT(5)
#define PWM_CLK_GATING      BIT(6)
#define PWM_MODE            BIT(7)
#define PWM_PULSE           BIT(8)
#define PWM_BYPASS          BIT(9)

#define PWM_RDY_BASE        28
#define PWM_RDY_OFFSET      1
#define PWM_RDY(ch)         BIT(PWM_RDY_BASE + PWM_RDY_OFFSET * (ch))

#define PWM_PRD(prd)        (((prd) - 1) << 16)
#define PWM_PRD_MASK        GENMASK(15, 0)
#define PWM_DTY_MASK        GENMASK(15, 0)

#define PWM_REG_PRD(reg)            ((((reg) >> 16) & PWM_PRD_MASK) + 1)
#define PWM_REG_DTY(reg)            ((reg) & PWM_DTY_MASK)
#define PWM_REG_PRESCAL(reg, chan)  (((reg) >> ((chan) * PWMCH_OFFSET)) & PWM_PRESCAL_MASK)
#define BIT_CH(bit, chan)           ((bit) << ((chan) * PWMCH_OFFSET))

static const u32 prescaler_table[] = {
  120,
  180,
  240,
  360,
  480,
  0,
  0,
  0,
  12000,
  24000,
  36000,
  48000,
  72000,
  0,
  0,
  0, /* Actually 1 but tested separately */
};

struct suniv_pwm_data {
  bool has_prescaler_bypass;
  bool has_rdy;
  unsigned int npwm;
};

struct suniv_pwm_chip {
  struct pwm_chip chip;
  struct clk *clk;
  void __iomem *base;
  spinlock_t ctrl_lock;
  const struct suniv_pwm_data *data;
  unsigned long next_period[2];
  bool needs_delay[2];
};

static int major = -1;
static int motor_ver = 3;
module_param(motor_ver,int,0660);
static struct cdev mycdev;
static struct class *myclass = NULL;

static inline struct suniv_pwm_chip *to_suniv_pwm_chip(struct pwm_chip *chip)
{
  return container_of(chip, struct suniv_pwm_chip, chip);
}

static inline u32 suniv_pwm_readl(struct suniv_pwm_chip *chip, unsigned long offset)
{
  return readl(chip->base + offset);
}

static inline void suniv_pwm_writel(struct suniv_pwm_chip *chip, u32 val, unsigned long offset)
{
  writel(val, chip->base + offset);
}

static void suniv_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm, struct pwm_state *state)
{
  struct suniv_pwm_chip *suniv_pwm = to_suniv_pwm_chip(chip);
  u64 clk_rate, tmp;
  u32 val;
  unsigned int prescaler;

  clk_rate = clk_get_rate(suniv_pwm->clk);
  val = suniv_pwm_readl(suniv_pwm, PWM_CTRL_REG);
  if ((val == PWM_PRESCAL_MASK) && suniv_pwm->data->has_prescaler_bypass) {
    prescaler = 1;
  }
  else {
    prescaler = prescaler_table[PWM_REG_PRESCAL(val, pwm->hwpwm)];
  }

  if (prescaler == 0) {
    return;
  }

  if (val & BIT_CH(PWM_ACT_STATE, pwm->hwpwm)) {
    state->polarity = PWM_POLARITY_NORMAL;
  }
  else {
    state->polarity = PWM_POLARITY_INVERSED;
  }

  if (val & BIT_CH(PWM_CLK_GATING | PWM_EN, pwm->hwpwm)) {
    state->enabled = true;
  }
  else {
    state->enabled = false;
  }

  val = suniv_pwm_readl(suniv_pwm, PWM_CH_PRD(pwm->hwpwm));
  tmp = prescaler * NSEC_PER_SEC * PWM_REG_DTY(val);
  state->duty_cycle = DIV_ROUND_CLOSEST_ULL(tmp, clk_rate);
  tmp = prescaler * NSEC_PER_SEC * PWM_REG_PRD(val);
  state->period = DIV_ROUND_CLOSEST_ULL(tmp, clk_rate);
}

static int suniv_pwm_calculate(struct suniv_pwm_chip *suniv_pwm, struct pwm_state *state, u32 *dty, u32 *prd, unsigned int *prsclr)
{
  u64 clk_rate, div = 0;
  unsigned int pval, prescaler = 0;

  clk_rate = clk_get_rate(suniv_pwm->clk);
  if (suniv_pwm->data->has_prescaler_bypass) {
    // first, test without any prescaler when available
    prescaler = PWM_PRESCAL_MASK;
    pval = 1;
    /*
     * When not using any prescaler, the clock period in nanoseconds
     * is not an integer so round it half up instead of
     * truncating to get less surprising values.
     */
    div = clk_rate * state->period + NSEC_PER_SEC / 2;
    do_div(div, NSEC_PER_SEC);
    if (div - 1 > PWM_PRD_MASK) {
      prescaler = 0;
    }
  }

  if (prescaler == 0) {
    /* Go up from the first divider */
    for (prescaler = 0; prescaler < PWM_PRESCAL_MASK; prescaler++) {
      if (!prescaler_table[prescaler]) {
        continue;
      }
      pval = prescaler_table[prescaler];
      div = clk_rate;
      do_div(div, pval);
      div = div * state->period;
      do_div(div, NSEC_PER_SEC);
      if (div - 1 <= PWM_PRD_MASK) {
        break;
      }
    }

    if (div - 1 > PWM_PRD_MASK) {
      return -EINVAL;
    }
  }

  *prd = div;
  div *= state->duty_cycle;
  do_div(div, state->period);
  *dty = div;
  *prsclr = prescaler;

  div = (u64)pval * NSEC_PER_SEC * *prd;
  state->period = DIV_ROUND_CLOSEST_ULL(div, clk_rate);
  div = (u64)pval * NSEC_PER_SEC * *dty;
  state->duty_cycle = DIV_ROUND_CLOSEST_ULL(div, clk_rate);
  return 0;
}

static int suniv_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm, const struct pwm_state *state)
{
  int ret;
  u32 ctrl;
  unsigned long now;
  unsigned int delay_us;
  unsigned long long ull_period;
  unsigned long ul_period;
  struct pwm_state cstate;
  struct suniv_pwm_chip *suniv_pwm = to_suniv_pwm_chip(chip);

  pwm_get_state(pwm, &cstate);
  if (!cstate.enabled) {
    ret = clk_prepare_enable(suniv_pwm->clk);
    if (ret) {
      dev_err(chip->dev, "failed to enable PWM clock\n");
      return ret;
    }
  }

  spin_lock(&suniv_pwm->ctrl_lock);
  ctrl = suniv_pwm_readl(suniv_pwm, PWM_CTRL_REG);

  if ((cstate.period != state->period) || (cstate.duty_cycle != state->duty_cycle)) {
    u32 period, duty, val;
    unsigned int prescaler;

    ret = suniv_pwm_calculate(suniv_pwm, state, &duty, &period, &prescaler);
    if (ret) {
      dev_err(chip->dev, "period exceeds the maximum value\n");
      spin_unlock(&suniv_pwm->ctrl_lock);
      if (!cstate.enabled) {
        clk_disable_unprepare(suniv_pwm->clk);
      }
      return ret;
    }

    if (PWM_REG_PRESCAL(ctrl, pwm->hwpwm) != prescaler) {
      // prescaler changed, the clock has to be gated
      ctrl &= ~BIT_CH(PWM_CLK_GATING, pwm->hwpwm);
      suniv_pwm_writel(suniv_pwm, ctrl, PWM_CTRL_REG);
      ctrl &= ~BIT_CH(PWM_PRESCAL_MASK, pwm->hwpwm);
      ctrl |= BIT_CH(prescaler, pwm->hwpwm);
    }

    val = (duty & PWM_DTY_MASK) | PWM_PRD(period);
    suniv_pwm_writel(suniv_pwm, val, PWM_CH_PRD(pwm->hwpwm));

    ull_period = cstate.period;
    ul_period = (unsigned long)ull_period;
    suniv_pwm->next_period[pwm->hwpwm] = jiffies + usecs_to_jiffies(ul_period / 1000 + 1);
    suniv_pwm->needs_delay[pwm->hwpwm] = true;
  }

  if (state->polarity != PWM_POLARITY_NORMAL) {
    ctrl &= ~BIT_CH(PWM_ACT_STATE, pwm->hwpwm);
  }
  else {
    ctrl |= BIT_CH(PWM_ACT_STATE, pwm->hwpwm);
  }

  ctrl |= BIT_CH(PWM_CLK_GATING, pwm->hwpwm);
  if (state->enabled) {
    ctrl |= BIT_CH(PWM_EN, pwm->hwpwm);
  } 
  else if (!suniv_pwm->needs_delay[pwm->hwpwm]) {
    ctrl &= ~BIT_CH(PWM_EN, pwm->hwpwm);
    ctrl &= ~BIT_CH(PWM_CLK_GATING, pwm->hwpwm);
  }
  suniv_pwm_writel(suniv_pwm, ctrl, PWM_CTRL_REG);
  spin_unlock(&suniv_pwm->ctrl_lock);

  if (state->enabled) {
    return 0;
  }

  if (!suniv_pwm->needs_delay[pwm->hwpwm]) {
    clk_disable_unprepare(suniv_pwm->clk);
    return 0;
  }

  // We need a full period to elapse before disabling the channel.
  now = jiffies;
  if (suniv_pwm->needs_delay[pwm->hwpwm] && time_before(now, suniv_pwm->next_period[pwm->hwpwm])) {
    delay_us = jiffies_to_usecs(suniv_pwm->next_period[pwm->hwpwm] - now);
    if ((delay_us / 500) > MAX_UDELAY_MS) {
      msleep(delay_us / 1000 + 1);
    }
    else {
      usleep_range(delay_us, delay_us * 2);
    }
  }
  suniv_pwm->needs_delay[pwm->hwpwm] = false;

  spin_lock(&suniv_pwm->ctrl_lock);
  ctrl = suniv_pwm_readl(suniv_pwm, PWM_CTRL_REG);
  ctrl &= ~BIT_CH(PWM_CLK_GATING, pwm->hwpwm);
  ctrl &= ~BIT_CH(PWM_EN, pwm->hwpwm);
  suniv_pwm_writel(suniv_pwm, ctrl, PWM_CTRL_REG);
  spin_unlock(&suniv_pwm->ctrl_lock);
  clk_disable_unprepare(suniv_pwm->clk);
  return 0;
}

static const struct pwm_ops suniv_pwm_ops = {
  .apply = suniv_pwm_apply,
  .get_state = suniv_pwm_get_state,
  .owner = THIS_MODULE,
};

static const struct suniv_pwm_data suniv_pwm_data_a10 = {
  .has_prescaler_bypass = false,
  .has_rdy = false,
  .npwm = 2,
};

static const struct of_device_id suniv_pwm_dt_ids[] = {
  {
    .compatible = "allwinner,suniv-pwm",
    .data = &suniv_pwm_data_a10,
  }, {
  },
};
MODULE_DEVICE_TABLE(of, suniv_pwm_dt_ids);

static int get_motor_pin(int ver)
{
  switch(ver){
  case 1:
    return ((32 * 4) + 1);
  case 2: case 3: case 4:
    return ((32 * 4) + 12);
  }
  return -1;
}

static int do_gpio_request(int ver)
{
  const char *name = "miyoo_motor";
  int pin = get_motor_pin(ver);

  if(pin < 0){
    printk("failed to config miyoo motor for this miyoo device\n");
    return -1;
  }
  if(gpio_request(pin, name) < 0){
    printk("failed to request gpio, ver:%d, pin:%d, name:%s\n", ver, pin, name);
    return -1;
  }
  gpio_direction_output(pin, 1);
  printk("miyoo motor config as v%d\n", ver);
  return 0;
}

static void do_gpio_free(int ver)
{
  int pin = get_motor_pin(ver);
  
  if(pin > 0){
    gpio_free(pin);
  }
}

extern void MIYOO_RUMBLE(unsigned int rumble){
  int pin = get_motor_pin(motor_ver);
  if(pin > 0){
    if(rumble) {
      gpio_set_value(pin, 0);
    } else {
      gpio_set_value(pin, 1);
    }
  }
}

EXPORT_SYMBOL_GPL(MIYOO_RUMBLE);


static int myopen(struct inode *inode, struct file *file)
{
  return 0;
}

static int myclose(struct inode *inode, struct file *file)
{
  return 0;
}

static long myioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
  int pin = get_motor_pin(motor_ver);

  switch(cmd){
  case MIYOO_VIR_SET_MODE:
    if(pin > 0){
      gpio_set_value(pin, arg ? 1 : 0);
    }
    break;
  case MIYOO_VIR_SET_VER:
    if((arg == motor_ver) || (pin < 0)){
      break;
    }

    do_gpio_free(motor_ver);
    motor_ver = arg;
    do_gpio_request(motor_ver);
    break;
  }
  return 0;
}

static const struct file_operations myfops = {
  .owner = THIS_MODULE,
  .open = myopen,
  .release = myclose,
  .unlocked_ioctl = myioctl,
};

static int suniv_pwm_probe(struct platform_device *pdev)
{
  int ret;
  struct resource *res;
  struct suniv_pwm_chip *pwm;
  const struct of_device_id *match;

  match = of_match_device(suniv_pwm_dt_ids, &pdev->dev);
  pwm = devm_kzalloc(&pdev->dev, sizeof(*pwm), GFP_KERNEL);
  if (!pwm) {
    return -ENOMEM;
  }

  res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  pwm->base = devm_ioremap_resource(&pdev->dev, res);
  if (IS_ERR(pwm->base)) {
    return PTR_ERR(pwm->base);
  }

  pwm->clk = devm_clk_get(&pdev->dev, NULL);
  if (IS_ERR(pwm->clk)) {
    return PTR_ERR(pwm->clk);
  }

  pwm->data = match->data;
  pwm->chip.dev = &pdev->dev;
  pwm->chip.ops = &suniv_pwm_ops;
  pwm->chip.base = -1;
  pwm->chip.npwm = pwm->data->npwm;
  pwm->chip.of_xlate = of_pwm_xlate_with_flags;
  pwm->chip.of_pwm_n_cells = 3;
  spin_lock_init(&pwm->ctrl_lock);

  ret = pwmchip_add(&pwm->chip);
  if (ret < 0) {
    dev_err(&pdev->dev, "failed to add PWM chip: %d\n", ret);
    return ret;
  }
  platform_set_drvdata(pdev, pwm);

  alloc_chrdev_region(&major, 0, 1, "miyoo_vir");
  myclass = class_create(THIS_MODULE, "miyoo_vir");
  device_create(myclass, NULL, major, NULL, "miyoo_vir");
  cdev_init(&mycdev, &myfops);
  cdev_add(&mycdev, major, 1); 

// motor_ver = 1;
  do_gpio_request(motor_ver);
  return 0;
}

static int suniv_pwm_remove(struct platform_device *pdev)
{
  struct suniv_pwm_chip *pwm = platform_get_drvdata(pdev);
  pwmchip_remove(&pwm->chip);
  return 0;
}

static struct platform_driver suniv_pwm_driver = {
  .driver = {
    .name = "suniv-pwm",
    .of_match_table = suniv_pwm_dt_ids,
  },
  .probe = suniv_pwm_probe,
  .remove = suniv_pwm_remove,
};
module_platform_driver(suniv_pwm_driver);

MODULE_AUTHOR("Steward Fu <steward.fu@gmail.com>");
MODULE_DESCRIPTION("Allwinner f1c100s PWM driver");
MODULE_LICENSE("GPL");
