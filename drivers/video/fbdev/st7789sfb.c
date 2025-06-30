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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/console.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/lcm.h>
#include <linux/clk-provider.h>
#include <video/of_display_timing.h>
#include <linux/gpio.h>
#include <linux/omapfb.h>
#include <linux/compiler.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/gpio.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/initval.h>
#include <sound/dmaengine_pcm.h>
#include <linux/gpio.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/gpio.h>
#include <asm/arch-suniv/cpu.h>
#include <asm/arch-suniv/dma.h>
#include <asm/arch-suniv/gpio.h>
#include <asm/arch-suniv/intc.h>
#include <asm/arch-suniv/lcdc.h>
#include <asm/arch-suniv/debe.h>
#include <asm/arch-suniv/codec.h>
#include <asm/arch-suniv/clock.h>
#include <asm/arch-suniv/common.h>

//#define DEBUG
#define TIMEOUT_NS 1000000
#define PALETTE_SIZE 256
#define DRIVER_NAME  "ST7789S-fb"
#define MIYOO_FB0_PUT_OSD     _IOWR(0x100, 0, unsigned long)
#define MIYOO_FB0_SET_MODE    _IOWR(0x101, 0, unsigned long)
#define MIYOO_FB0_GET_VER     _IOWR(0x102, 0, unsigned long)
#define MIYOO_FB0_SET_FPBP    _IOWR(0x104, 0, unsigned long)
#define MIYOO_FB0_SET_TEFIX   _IOWR(0x106, 0, unsigned long)
#define MIYOO_FB0_GET_TEFIX   _IOWR(0x107, 0, unsigned long)
DECLARE_WAIT_QUEUE_HEAD(wait_vsync_queue);

static bool flip=false;
module_param(flip,bool,0660);

static bool lowcurrent=false;
module_param(lowcurrent,bool,0660);

static int tefix = 0; //DEFAULT_TEFIX
module_param(tefix,int,0660);

static bool invert=false;
module_param(invert,bool,0660);

struct myfb_app{
    uint32_t yoffset;
    uint32_t vsync_count;
};

struct myfb_par {
    struct device *dev;
    struct platform_device *pdev;

    resource_size_t p_palette_base;
    unsigned short *v_palette_base;

    void *vram_virt;
    uint32_t vram_size;
    dma_addr_t vram_phys;
    struct myfb_app *app_virt;

    int bpp;
    int lcdc_irq;
    int gpio_irq;
    int lcdc_ready;
    u32 pseudo_palette[16];
    struct fb_videomode mode;
};

struct suniv_iomm {
    uint8_t *dma;
    uint8_t *ccm;
    uint8_t *gpio;
    uint8_t *lcdc;
    uint8_t *debe;
    uint8_t *intc;
    uint8_t *timer;
};

static int major = -1;
static struct cdev mycdev;
static struct class *myclass = NULL;
static uint32_t miyoo_ver = 2;
struct timer_list mytimer;
static struct suniv_iomm iomm={0};
static struct myfb_par *mypar=NULL;
static struct fb_var_screeninfo myfb_var={0};
ktime_t start;
uint16_t x, i, scanline, vsync;
uint32_t mycpu_clock;
uint32_t video_clock;

static struct fb_fix_screeninfo myfb_fix = {
        .id = DRIVER_NAME,
        .type = FB_TYPE_PACKED_PIXELS,
        .type_aux = 0,
        .visual = FB_VISUAL_TRUECOLOR,
        .xpanstep = 0,
        .ypanstep = 1,
        .ywrapstep = 0,
        .accel = FB_ACCEL_NONE
};

static int wait_for_vsync(struct myfb_par *par)
{
    uint32_t count = par->app_virt->vsync_count;
    long t = wait_event_interruptible_timeout(wait_vsync_queue, count != par->app_virt->vsync_count, HZ / 10);
    return t > 0 ? 0 : (t < 0 ? (int)t : -ETIMEDOUT);
}

static void suniv_gpio_init(void)
{
    uint32_t r=0;

    r = readl(iomm.gpio + PD_CFG0);
    r&= 0x0000000f;
    r|= 0x22222220;
    writel(r, iomm.gpio + PD_CFG0);

    r = readl(iomm.gpio + PD_CFG1);
    r&= 0x000000f0;
    r|= 0x22222202;
    writel(r, iomm.gpio + PD_CFG1);

    r = readl(iomm.gpio + PD_CFG2);
    r&= 0x00000000;
    r|= 0x22222222;
    writel(r, iomm.gpio + PD_CFG2);

    if (lowcurrent) {
    writel(0x00000000, iomm.gpio + PD_DRV0);
    }

    r = readl(iomm.gpio + PD_PUL1);
    r&= 0xfffff0ff;
    r|= 0x00000500;
    writel(r, iomm.gpio + PD_PUL1);

    r = readl(iomm.gpio + PE_CFG1);
    r&= 0xffff0fff;
    r|= 0x00001000;
    writel(r, iomm.gpio + PE_CFG1);
    writel(0xffffffff, iomm.gpio + PE_DATA);
}

static uint32_t lcdc_wait_busy(void)
{
    uint32_t cnt=0;

    suniv_setbits(iomm.lcdc + TCON0_CPU_IF_REG, (1 << 0));
    ndelay(10);
    while (1) {
        if (readl(iomm.lcdc + TCON0_CPU_IF_REG) & 0x00c00000) {
            if (cnt > 200) {
                return -1;
            } else {
                cnt+= 1;
            }
        }
        break;
    }
    return 0;
}

static uint32_t extend_16b_to_24b(uint32_t value)
{
    return ((value & 0xfc00) << 8) | ((value & 0x0300) << 6) | ((value & 0x00e0) << 5) | ((value & 0x001f) << 3);
}

static void lcdc_wr(uint8_t is_data, uint32_t data)
{
    while (lcdc_wait_busy());
    if (is_data) {
        suniv_setbits(iomm.lcdc + TCON0_CPU_IF_REG, (1 << 25));
    } else {
        suniv_clrbits(iomm.lcdc + TCON0_CPU_IF_REG, (1 << 25));
    }
    while (lcdc_wait_busy());
    writel(extend_16b_to_24b(data), iomm.lcdc + TCON0_CPU_WR_REG);
}

static void lcdc_wr_cmd(uint32_t cmd)
{
    lcdc_wr(0, cmd);
}

static void lcdc_wr_dat(uint32_t cmd)
{
    lcdc_wr(1, cmd);
}

static uint32_t extend_24b_to_16b(uint32_t value)
{
    return ((value & 0xfc0000) >> 8) | ((value & 0xc000) >> 6) | ((value & 0x1c00) >> 5) | ((value & 0x00f8) >> 3);
}

static uint32_t lcdc_rd_dat(void)
{
	while (lcdc_wait_busy());
	suniv_setbits(iomm.lcdc + TCON0_CPU_IF_REG, (1 << 25)); // ca=1
	while (lcdc_wait_busy());
	return extend_24b_to_16b(readl(iomm.lcdc + TCON0_CPU_RD_REG));
}

static void refresh_lcd(struct myfb_par *par)
{
    if (par->lcdc_ready) {
        lcdc_wr_cmd(0x2c);
  
		if(par->app_virt->yoffset == 0) {
            suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
            suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 9));
            suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 10));
            suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 11));
        }
        else if(par->app_virt->yoffset == 240) {
            suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
            suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 9));
            suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 10));
            suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 11));
        }
        else if(par->app_virt->yoffset == 480) {
            suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
            suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 9));
            suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 10));
            suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 11));
        }
        else if(par->app_virt->yoffset == 720) {
            suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
            suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 9));
            suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 10));
            suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 11));
        }
	    suniv_setbits(iomm.debe + DEBE_REGBUFF_CTRL_REG, (1 << 0));
    }
}

static irqreturn_t gpio_irq_handler(int irq, void *arg)
{
    refresh_lcd(arg);
    return IRQ_HANDLED;
}

uint16_t st7789_get_scanline(void) {
	uint8_t buf[2] = {0};
	lcdc_wr_cmd(0x45);
	lcdc_rd_dat();
	buf[0] = lcdc_rd_dat();
	buf[1] = lcdc_rd_dat();

	return  ((uint16_t)buf[0] << 8) | buf[1];
}

static irqreturn_t lcdc_irq_handler(int irq, void *arg)
{
	if (tefix != 0) {
      suniv_clrbits(iomm.lcdc + TCON0_CPU_IF_REG, (1 << 28));
          lcdc_wr_cmd(0x45);
          lcdc_rd_dat();
          lcdc_rd_dat();
	      start = ktime_get();
	      while (st7789_get_scanline() == 0) {
		      if (ktime_to_ns(ktime_sub(ktime_get(), start)) > TIMEOUT_NS) {
			      suniv_setbits(iomm.lcdc + TCON0_CPU_IF_REG, (1 << 28));
			      suniv_clrbits(iomm.lcdc + TCON_INT_REG0, (1 << 15));
			      return IRQ_HANDLED;
		      }
		      cpu_relax();
	      }
	      refresh_lcd(arg);
    suniv_setbits(iomm.lcdc + TCON0_CPU_IF_REG, (1 << 28));
    suniv_clrbits(iomm.lcdc + TCON_INT_REG0, (1 << 15));
    return IRQ_HANDLED;
	}
    refresh_lcd(arg);
    suniv_clrbits(iomm.lcdc + TCON_INT_REG0, (1 << 15));
    return IRQ_HANDLED;	
}

static void init_lcd(void)
{
    suniv_gpio_init();
    suniv_clrbits(iomm.lcdc + PE_DATA, (1 << 11));
    mdelay(150);
    suniv_setbits(iomm.lcdc + PE_DATA, (1 << 11));
    mdelay(150);

    lcdc_wr_cmd(0x11);
    mdelay(250);
                  
    lcdc_wr_cmd(0x36);
    if (flip) {
        lcdc_wr_dat(0x70); //screen direction //0x70 for 3.5, 0xB0 for pg
    } else {
        lcdc_wr_dat(0xB0); //screen direction //0x70 for 3.5, 0xB0 for pg
    }
//    lcdc_wr_cmd(0x3a);
//    lcdc_wr_dat(0x05);
      
    lcdc_wr_cmd(0x2a);
    lcdc_wr_dat(0x00);
    lcdc_wr_dat(0x00);
    lcdc_wr_dat(0x01);
    lcdc_wr_dat(0x3f);

    lcdc_wr_cmd(0x2b);
    lcdc_wr_dat(0x00);
    lcdc_wr_dat(0x00);
    lcdc_wr_dat(0x00);
    lcdc_wr_dat(0xef);
        
    // ST7789S Frame rate setting
	lcdc_wr_cmd(0xb2);
	if (tefix == 3) {
		lcdc_wr_dat(8); // bp 0x0a
		lcdc_wr_dat(122); // fp 0x0b
    } else if (tefix == 2) {
        lcdc_wr_dat(8); // bp 0x0a
        lcdc_wr_dat(120); // fp 0x0b
	} else if (tefix == 1) {
		lcdc_wr_dat(90); // bp 0x0a
		lcdc_wr_dat(20); // fp 0x0b
	} else {
        lcdc_wr_dat(9); // bp 0x0a
        lcdc_wr_dat(10); // fp 0x0b
    }
		lcdc_wr_dat(0x00);        			
		lcdc_wr_dat(0x33);
		lcdc_wr_dat(0x33);

//    // Gate Control
//    lcdc_wr_cmd(0xb7);
//    lcdc_wr_dat(0x35);
//
//    // ?
//    lcdc_wr_cmd(0xb8);
//    lcdc_wr_dat(0x2f);
//    lcdc_wr_dat(0x2b);
//    lcdc_wr_dat(0x2f);
//
//    // ST7789S Power setting
//    lcdc_wr_cmd(0xbb);
//    lcdc_wr_dat(0x15);
//
//    lcdc_wr_cmd(0xc0);
//    lcdc_wr_dat(0x3C);
//
//    lcdc_wr_cmd(0xc2);
//    lcdc_wr_dat(0x01);
//
//    lcdc_wr_cmd(0xc3);
//    lcdc_wr_dat(0x13); // or 0x0b?
//
//    lcdc_wr_cmd(0xc4);
//    lcdc_wr_dat(0x20);
//
    if (invert) {
        lcdc_wr_cmd(0x21); // Display Inversion On (INVON for colors)
    } else {
        lcdc_wr_cmd(0x20); //  Display Inversion Off (INVOFF for colors)
    }

    lcdc_wr_cmd(0xc6);
    if (tefix == 3)
        lcdc_wr_dat(0x03); // 0x04, 0x1f
    else if (tefix == 2)
        lcdc_wr_dat(0x04);
    else if (tefix == 1)
        lcdc_wr_dat(0x03);
    else
        lcdc_wr_dat(0x03); // 0x04, 0x1f
//
//    lcdc_wr_cmd(0xd0);
//    lcdc_wr_dat(0xa4);
//    lcdc_wr_dat(0xa1);
//
//    lcdc_wr_cmd(0xe8);
//    lcdc_wr_dat(0x03);
//
//    lcdc_wr_cmd(0xe9);
//    lcdc_wr_dat(0x0d);
//    lcdc_wr_dat(0x12);
//    lcdc_wr_dat(0x00);
//
//    // ST7789S gamma setting
//    lcdc_wr_cmd(0xe0);
//    lcdc_wr_dat(0x70);
//    lcdc_wr_dat(0x00);
//    lcdc_wr_dat(0x06);
//    lcdc_wr_dat(0x09);
//    lcdc_wr_dat(0x0b);
//    lcdc_wr_dat(0x2a);
//    lcdc_wr_dat(0x3c);
//    lcdc_wr_dat(0x33);
//    lcdc_wr_dat(0x4b);
//    lcdc_wr_dat(0x08);
//    lcdc_wr_dat(0x16);
//    lcdc_wr_dat(0x14);
//    lcdc_wr_dat(0x2a);
//    lcdc_wr_dat(0x23);
//
//    lcdc_wr_cmd(0xe1);
//    lcdc_wr_dat(0xd0);
//    lcdc_wr_dat(0x00);
//    lcdc_wr_dat(0x06);
//    lcdc_wr_dat(0x09);
//    lcdc_wr_dat(0x0b);
//    lcdc_wr_dat(0x29);
//    lcdc_wr_dat(0x36);
//    lcdc_wr_dat(0x54);
//    lcdc_wr_dat(0x4b);
//    lcdc_wr_dat(0x0d);
//    lcdc_wr_dat(0x16);
//    lcdc_wr_dat(0x14);
//    lcdc_wr_dat(0x28);
//    lcdc_wr_dat(0x22);

    mdelay(50);
    lcdc_wr_cmd(0x29);
    mdelay(50);
    lcdc_wr_cmd(0x2c);
    mdelay(100);

    mypar->app_virt->yoffset = 0;
    memset(mypar->vram_virt, 0, 320*240*4);
}

static void suniv_fb_addr_init(struct myfb_par *par)
{
    writel((uint32_t)(par->vram_phys + 320*240*2*0) << 3, iomm.debe + DEBE_LAY0_FB_ADDR_REG);
    writel((uint32_t)(par->vram_phys + 320*240*2*1) << 3, iomm.debe + DEBE_LAY1_FB_ADDR_REG);
    writel((uint32_t)(par->vram_phys + 320*240*2*2) << 3, iomm.debe + DEBE_LAY2_FB_ADDR_REG);
    writel((uint32_t)(par->vram_phys + 320*240*2*3) << 3, iomm.debe + DEBE_LAY3_FB_ADDR_REG);

    writel((uint32_t)(par->vram_phys + 320*240*2*0) >> 29, iomm.debe + DEBE_LAY0_FB_HI_ADDR_REG);
    writel((uint32_t)(par->vram_phys + 320*240*2*1) >> 29, iomm.debe + DEBE_LAY1_FB_HI_ADDR_REG);
    writel((uint32_t)(par->vram_phys + 320*240*2*2) >> 29, iomm.debe + DEBE_LAY2_FB_HI_ADDR_REG);
    writel((uint32_t)(par->vram_phys + 320*240*2*3) >> 29, iomm.debe + DEBE_LAY3_FB_HI_ADDR_REG);	
}

static void suniv_lcdc_init(unsigned long xres, unsigned long yres)
{
    uint32_t ret=0, bp=0, total=0;
    uint32_t h_front_porch = 8;
    uint32_t h_back_porch = 8;
    uint32_t h_sync_len = 1;
    uint32_t v_front_porch = 8;
    uint32_t v_back_porch = 8;
    uint32_t v_sync_len = 1;
    if (tefix == 3) {
        v_front_porch = 10;
        v_back_porch = 116;
    } else if (tefix == 2) {
        v_front_porch = 10;
        v_back_porch = 137;
    } else if (tefix == 1) {
        h_front_porch = 45;
        h_back_porch = 45;
	    v_front_porch = 4;
        v_back_porch = 16;
        v_sync_len = 3;
        h_sync_len = 3;
	}

    writel(0, iomm.lcdc + TCON_CTRL_REG);
    writel(0, iomm.lcdc + TCON_INT_REG0);
    ret = readl(iomm.lcdc + TCON_CLK_CTRL_REG);
    ret&= ~(0xf << 28);
    writel(ret, iomm.lcdc + TCON_CLK_CTRL_REG);
    writel(0xffffffff, iomm.lcdc + TCON0_IO_CTRL_REG1);
    writel(0xffffffff, iomm.lcdc + TCON1_IO_CTRL_REG1);

    suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 0));
    writel(xres << 4, iomm.debe + DEBE_LAY0_LINEWIDTH_REG);
    writel(xres << 4, iomm.debe + DEBE_LAY1_LINEWIDTH_REG);
    writel(xres << 4, iomm.debe + DEBE_LAY2_LINEWIDTH_REG);
    writel(xres << 4, iomm.debe + DEBE_LAY3_LINEWIDTH_REG);
    writel((((yres) - 1) << 16) | (((xres) - 1) << 0), iomm.debe + DEBE_DISP_SIZE_REG);
    writel((((yres) - 1) << 16) | (((xres) - 1) << 0), iomm.debe + DEBE_LAY0_SIZE_REG);
    writel((((yres) - 1) << 16) | (((xres) - 1) << 0), iomm.debe + DEBE_LAY1_SIZE_REG);
    writel((((yres) - 1) << 16) | (((xres) - 1) << 0), iomm.debe + DEBE_LAY2_SIZE_REG);
    writel((((yres) - 1) << 16) | (((xres) - 1) << 0), iomm.debe + DEBE_LAY3_SIZE_REG);
    writel((5 << 8), iomm.debe + DEBE_LAY0_ATT_CTRL_REG1);
    writel((5 << 8), iomm.debe + DEBE_LAY1_ATT_CTRL_REG1);
    writel((5 << 8), iomm.debe + DEBE_LAY2_ATT_CTRL_REG1);
    writel((5 << 8), iomm.debe + DEBE_LAY3_ATT_CTRL_REG1);
    suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
    suniv_setbits(iomm.debe + DEBE_REGBUFF_CTRL_REG, (1 << 1));
    suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 1));

    ret = readl(iomm.lcdc + TCON_CTRL_REG);
    ret&= ~(1 << 0);
    writel(ret, iomm.lcdc + TCON_CTRL_REG);
    ret = (v_front_porch + v_back_porch + v_sync_len);

    writel((1 << 31) | ((ret & 0x1f) << 4) | (1 << 24), iomm.lcdc + TCON0_CTRL_REG);
    writel((0xf << 28) | (6 << 0), iomm.lcdc + TCON_CLK_CTRL_REG); //6, 15, 25
    writel((4 << 29) | (1 << 26), iomm.lcdc + TCON0_CPU_IF_REG);
    writel((1 << 28), iomm.lcdc + TCON0_IO_CTRL_REG0);

    writel(((xres - 1) << 16) | ((yres - 1) << 0), iomm.lcdc + TCON0_BASIC_TIMING_REG0);
    bp = h_sync_len + h_back_porch;
    total = xres * 1 + h_front_porch + bp;
    writel(((total - 1) << 16) | ((h_back_porch) << 0), iomm.lcdc + TCON0_BASIC_TIMING_REG1);
    bp = v_sync_len + v_back_porch;
    total = yres + v_front_porch + bp;
    writel(((total * 2) << 16) | ((v_back_porch) << 0), iomm.lcdc + TCON0_BASIC_TIMING_REG2);
    writel(((h_sync_len) << 16) | ((v_sync_len) << 0), iomm.lcdc + TCON0_BASIC_TIMING_REG3);
    writel(0, iomm.lcdc + TCON0_HV_TIMING_REG);
    writel(0, iomm.lcdc + TCON0_IO_CTRL_REG1);

    writel(0, iomm.lcdc + TCON0_HV_TIMING_REG);
    writel(0, iomm.lcdc + TCON0_IO_CTRL_REG1);

    suniv_setbits(iomm.lcdc + TCON_CTRL_REG, (1 << 31));
    init_lcd();
    suniv_setbits(iomm.lcdc + TCON_INT_REG0, (1 << 31));
    suniv_setbits(iomm.lcdc + TCON0_CPU_IF_REG, (1 << 28));
}

static void suniv_enable_irq(struct myfb_par *par)
{
    int ret=0;

    par->gpio_irq = gpio_to_irq(((32 * 4) + 10));
    if (par->gpio_irq < 0) {
        printk("%s, failed to get irq number for gpio irq\n", __func__);
    } else {
        ret = request_irq(par->gpio_irq, gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_irq", par);
        if (ret) {
            printk("%s, failed to register gpio interrupt(%d)\n", __func__, par->gpio_irq);
        }
    }

        par->lcdc_irq = platform_get_irq(par->pdev, 0);
        if (par->lcdc_irq < 0) {
            printk("%s, failed to get irq number for lcdc irq\n", __func__);
        } else {
            ret = request_irq(par->lcdc_irq, lcdc_irq_handler, IRQF_SHARED, "lcdc_irq", par);
            if (ret) {
                printk("%s, failed to register lcdc interrupt(%d)\n", __func__, par->lcdc_irq);
            }
        }

}

static void suniv_cpu_init(struct myfb_par *par)
{
    uint32_t ret, i;
    if (tefix == 3 || tefix == 2) {
        writel(0x91001303, iomm.ccm + PLL_VIDEO_CTRL_REG);
    } else {
        writel(0x91001107, iomm.ccm + PLL_VIDEO_CTRL_REG);
    }
    while ((readl(iomm.ccm + PLL_VIDEO_CTRL_REG) & (1 << 28)) == 0){}
    while ((readl(iomm.ccm + PLL_PERIPH_CTRL_REG) & (1 << 28)) == 0){}

    ret = readl(iomm.ccm + DRAM_GATING_REG);
    ret|= (1 << 26) | (1 << 24);
    writel(ret, iomm.ccm + DRAM_GATING_REG);

    suniv_setbits(iomm.ccm + FE_CLK_REG, (1 << 31));
    suniv_setbits(iomm.ccm + BE_CLK_REG, (1 << 31));
    suniv_setbits(iomm.ccm + TCON_CLK_REG, (1 << 31));
    suniv_setbits(iomm.ccm + BUS_CLK_GATING_REG1, (1 << 14) | (1 << 12) | (1 << 4));
    suniv_setbits(iomm.ccm + BUS_SOFT_RST_REG1, (1 << 14) | (1 << 12) | (1 << 4));
    for (i=0x0800; i<0x1000; i+=4) {
        writel(0, iomm.debe + i);
    }
}



#define CNVT_TOHW(val, width) ((((val) << (width)) + 0x7FFF - (val)) >> 16)
static int myfb_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue, unsigned transp, struct fb_info *info)
{
    red = CNVT_TOHW(red, info->var.red.length);
    blue = CNVT_TOHW(blue, info->var.blue.length);
    green = CNVT_TOHW(green, info->var.green.length);
    ((u32*)(info->pseudo_palette))[regno] = (red << info->var.red.offset) | (green << info->var.green.offset) | (blue << info->var.blue.offset);
    return 0;
}
#undef CNVT_TOHW

static int myfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
    int bpp = var->bits_per_pixel >> 3;
    struct myfb_par *par = info->par;
    unsigned long line_size = var->xres_virtual * bpp;

    if ((var->xres != 320) || (var->yres != 240) || (var->bits_per_pixel != 16)) {
        return -EINVAL;
    }

    var->transp.offset = 0;
    var->transp.length = 0;
    var->red.offset = 11;
    var->red.length = 5;
    var->green.offset = 5;
    var->green.length = 6;
    var->blue.offset = 0;
    var->blue.length = 5;
    var->red.msb_right = 0;
    var->green.msb_right = 0;
    var->blue.msb_right = 0;
    var->transp.msb_right = 0;
    if (line_size * var->yres_virtual > par->vram_size) {
        var->yres_virtual = par->vram_size / line_size;
    }
    if (var->yres > var->yres_virtual) {
        var->yres = var->yres_virtual;
    }
    if (var->xres > var->xres_virtual) {
        var->xres = var->xres_virtual;
    }
    if (var->xres + var->xoffset > var->xres_virtual) {
        var->xoffset = var->xres_virtual - var->xres;
    }
    if (var->yres + var->yoffset > var->yres_virtual) {
        var->yoffset = var->yres_virtual - var->yres;
    }
    return 0;
}

static int myfb_set_par(struct fb_info *info)
{
    struct myfb_par *par = info->par;

    fb_var_to_videomode(&par->mode, &info->var);
    par->app_virt->yoffset = info->var.yoffset = 0;
    par->bpp = info->var.bits_per_pixel;
    info->fix.visual = FB_VISUAL_TRUECOLOR;
    info->fix.line_length = (par->mode.xres * par->bpp) / 8;
    writel((5 << 8), iomm.debe + DEBE_LAY0_ATT_CTRL_REG1);
    writel((5 << 8), iomm.debe + DEBE_LAY1_ATT_CTRL_REG1);
    writel((5 << 8), iomm.debe + DEBE_LAY2_ATT_CTRL_REG1);
    writel((5 << 8), iomm.debe + DEBE_LAY3_ATT_CTRL_REG1);
    return 0;
}

static int myfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
    struct myfb_par *par = info->par;
    switch (cmd) {
        case FBIO_WAITFORVSYNC:
            wait_for_vsync(par);
            break;			
    }
    return 0;
}

static int myfb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
    const unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
    const unsigned long size = vma->vm_end - vma->vm_start;

    if (offset + size > info->fix.smem_len) {
        return -EINVAL;
    }

    if (remap_pfn_range(vma, vma->vm_start, (info->fix.smem_start + offset) >> PAGE_SHIFT, size, vma->vm_page_prot)) {
        return -EAGAIN;
    }
    return 0;
}

static int myfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
    struct myfb_par *par = info->par;

    info->var.xoffset = var->xoffset;
    info->var.yoffset = var->yoffset;
    par->app_virt->yoffset = var->yoffset;
    return 0;
}

static struct fb_ops myfb_ops = {
        .owner          = THIS_MODULE,
        .fb_check_var   = myfb_check_var,
        .fb_set_par     = myfb_set_par,
        .fb_setcolreg   = myfb_setcolreg,
        .fb_pan_display = myfb_pan_display,
        .fb_ioctl       = myfb_ioctl,
        .fb_mmap        = myfb_mmap,

        .fb_fillrect  = sys_fillrect,
        .fb_copyarea  = sys_copyarea,
        .fb_imageblit = sys_imageblit,
};

static int myfb_probe(struct platform_device *device)
{
    int ret=0;
    struct fb_info *info=NULL;
    struct myfb_par *par=NULL;
    struct fb_videomode *mode=NULL;

    mode = devm_kzalloc(&device->dev, sizeof(struct fb_videomode), GFP_KERNEL);
    if (mode == NULL) {
        return -ENOMEM;
    }
    mode->name = "320x240";
    mode->xres = 320;
    mode->yres = 240;
    mode->vmode = FB_VMODE_NONINTERLACED;
    pm_runtime_enable(&device->dev);
    pm_runtime_get_sync(&device->dev);

    info = framebuffer_alloc(sizeof(struct myfb_par), &device->dev);
    if (!info) {
        return -ENOMEM;
    }

    par = info->par;
    par->pdev = device;
    par->dev = &device->dev;
    par->bpp = 16;
    fb_videomode_to_var(&myfb_var, mode);

    par->vram_size = (320 * 240 * 2 * 4) + 4096;
    par->vram_virt = dma_alloc_coherent(par->dev, par->vram_size, (resource_size_t*)&par->vram_phys, GFP_KERNEL | GFP_DMA);
    if (!par->vram_virt) {
        return -EINVAL;
    }
    info->screen_base = (char __iomem*)par->vram_virt;
    myfb_fix.smem_start = par->vram_phys;
    myfb_fix.smem_len = par->vram_size;
    myfb_fix.line_length = 320 * 2;
    par->app_virt = (struct myfb_app*)((uint8_t*)par->vram_virt + (320 * 240 * 2 * 4));

    par->v_palette_base = dma_alloc_coherent(par->dev, PALETTE_SIZE, (resource_size_t*)&par->p_palette_base, GFP_KERNEL | GFP_DMA);
    if (!par->v_palette_base) {
        return -EINVAL;
    }
    memset(par->v_palette_base, 0, PALETTE_SIZE);
    myfb_var.grayscale = 0;
    myfb_var.bits_per_pixel = par->bpp;

    info->flags = FBINFO_FLAG_DEFAULT;
    info->fix = myfb_fix;
    info->var = myfb_var;
    info->fbops = &myfb_ops;
    info->pseudo_palette = par->pseudo_palette;
    info->fix.visual = (info->var.bits_per_pixel <= 8) ? FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;
    ret = fb_alloc_cmap(&info->cmap, PALETTE_SIZE, 0);
    if (ret) {
        return -EINVAL;
    }
    info->cmap.len = 32;

    myfb_var.activate = FB_ACTIVATE_FORCE;
    fb_set_var(info, &myfb_var);
    dev_set_drvdata(&device->dev, info);
    if (register_framebuffer(info) < 0) {
        return -EINVAL;
    }

    mypar = par;
    mypar->lcdc_ready = 0;
    mypar->app_virt->vsync_count = 0;
    for (ret=0; ret<of_clk_get_parent_count(device->dev.of_node); ret++) {
        clk_prepare_enable(of_clk_get(device->dev.of_node, ret));
    }
    init_waitqueue_head(&wait_vsync_queue);
    suniv_cpu_init(mypar);
	suniv_fb_addr_init(mypar);
    suniv_lcdc_init(mypar->mode.xres, mypar->mode.yres);
    mypar->lcdc_ready = 1;
    suniv_enable_irq(mypar);
    return 0;
}

static int myfb_remove(struct platform_device *dev)
{
    struct fb_info *info = dev_get_drvdata(&dev->dev);
    struct myfb_par *par = info->par;

    if (info) {
        free_irq(par->lcdc_irq, par);
        free_irq(par->gpio_irq, par);
        del_timer(&mytimer);
        flush_scheduled_work();
        unregister_framebuffer(info);
        fb_dealloc_cmap(&info->cmap);
        dma_free_coherent(NULL, PALETTE_SIZE, par->v_palette_base, par->p_palette_base);
        dma_free_coherent(NULL, par->vram_size, par->vram_virt, par->vram_phys);
        pm_runtime_put_sync(&dev->dev);
        pm_runtime_disable(&dev->dev);
        framebuffer_release(info);
    }
    return 0;
}

static int myfb_suspend(struct platform_device *dev, pm_message_t state)
{
    struct fb_info *info = platform_get_drvdata(dev);

    console_lock();
    fb_set_suspend(info, 1);
    pm_runtime_put_sync(&dev->dev);
    console_unlock();
    return 0;
}

static int myfb_resume(struct platform_device *dev)
{
    struct fb_info *info = platform_get_drvdata(dev);

    console_lock();
    pm_runtime_get_sync(&dev->dev);
    fb_set_suspend(info, 0);
    console_unlock();
    return 0;
}

static const struct of_device_id fb_of_match[] = {
        {
                .compatible = "allwinner,suniv-f1c100s-tcon0",
        },{}
};
MODULE_DEVICE_TABLE(of, fb_of_match);

static struct platform_driver fb_driver = {
        .probe    = myfb_probe,
        .remove   = myfb_remove,
        .suspend  = myfb_suspend,
        .resume   = myfb_resume,
        .driver = {
                .name   = DRIVER_NAME,
                .owner  = THIS_MODULE,
                .of_match_table = of_match_ptr(fb_of_match),
        },
};

static void suniv_ioremap(void)
{
    iomm.ccm = (uint8_t*)ioremap(SUNIV_CCM_BASE, 1024);
    iomm.gpio = (uint8_t*)ioremap(SUNIV_GPIO_BASE, 1024);
    iomm.lcdc = (uint8_t*)ioremap(SUNIV_LCDC_BASE, 1024);
    iomm.debe = (uint8_t*)ioremap(SUNIV_DEBE_BASE, 4096);
}

static void suniv_iounmap(void)
{
    iounmap(iomm.ccm);
    iounmap(iomm.gpio);
    iounmap(iomm.lcdc);
    iounmap(iomm.debe);
    iounmap(iomm.intc);
    iounmap(iomm.timer);
}

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
    int ret;
    int32_t w, bpp;

    switch (cmd) {
        case MIYOO_FB0_PUT_OSD:
            break;
        case MIYOO_FB0_SET_MODE:
            w = (arg >> 16);
            bpp = (arg & 0xffff);
            if ((bpp != 16)) {
                writel((5 << 8), iomm.debe + DEBE_LAY0_ATT_CTRL_REG1);
                writel((5 << 8), iomm.debe + DEBE_LAY1_ATT_CTRL_REG1);
            } else {
                writel((7 << 8) | 4, iomm.debe + DEBE_LAY0_ATT_CTRL_REG1);
                writel((7 << 8) | 4, iomm.debe + DEBE_LAY1_ATT_CTRL_REG1);
            }
            break;
        case MIYOO_FB0_GET_VER:
            w = copy_to_user((void*)arg, &miyoo_ver, sizeof(uint32_t));
            break;
        case MIYOO_FB0_SET_TEFIX:
            tefix = arg;
#if defined(DEBUG)
            printk("st7789sfb: set TE fix to: %d", (int)tefix);
#endif
            if (tefix == 3 || tefix == 2) {
            	writel(0x91001303, iomm.ccm + PLL_VIDEO_CTRL_REG);
            } else {
            	writel(0x91001107, iomm.ccm + PLL_VIDEO_CTRL_REG);
            }
            while ((readl(iomm.ccm + PLL_VIDEO_CTRL_REG) & (1 << 28)) == 0){};
            suniv_lcdc_init(320, 240);
            break;
        case MIYOO_FB0_GET_TEFIX:
            ret = copy_to_user((void*)arg, &tefix, sizeof(unsigned long));
#if defined(DEBUG)
	    video_clock = readl(iomm.ccm + PLL_VIDEO_CTRL_REG);
	    printk("VIDEO_clock set to 0x%x", (uint32_t)video_clock);
#endif
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

static int __init fb_init(void)
{
    alloc_chrdev_region(&major, 0, 1, "miyoo_fb0");
    myclass = class_create(THIS_MODULE, "miyoo_fb0");
    device_create(myclass, NULL, major, NULL, "miyoo_fb0");
    cdev_init(&mycdev, &myfops);
    cdev_add(&mycdev, major, 1);
    suniv_ioremap();
    return platform_driver_register(&fb_driver);
}

static void __exit fb_cleanup(void)
{
    suniv_iounmap();
    device_destroy(myclass, major);
    cdev_del(&mycdev);
    class_destroy(myclass);
    unregister_chrdev_region(major, 1);
    platform_driver_unregister(&fb_driver);
}

module_init(fb_init);
module_exit(fb_cleanup);

MODULE_DESCRIPTION("Framebuffer driver for ST7789S");
MODULE_AUTHOR("Steward Fu <steward.fu@gmail.com>");
MODULE_LICENSE("GPL");
