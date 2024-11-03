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
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch-suniv/cpu.h>
#include <asm/arch-suniv/dma.h>
#include <asm/arch-suniv/gpio.h>
#include <asm/arch-suniv/intc.h>
#include <asm/arch-suniv/lcdc.h>
#include <asm/arch-suniv/debe.h>
#include <asm/arch-suniv/clock.h>
#include <asm/arch-suniv/common.h>

#define MIYOO_FB0_PUT_OSD     _IOWR(0x100, 0, unsigned long)
#define MIYOO_FB0_SET_MODE    _IOWR(0x101, 0, unsigned long)
#define MIYOO_FB0_GET_VER     _IOWR(0x102, 0, unsigned long)
#define MIYOO_FB0_SET_FLIP    _IOWR(0x103, 0, unsigned long)
#define MIYOO_FB0_SET_FPBP    _IOWR(0x104, 0, unsigned long)
#define MIYOO_FB0_GET_FPBP    _IOWR(0x105, 0, unsigned long)

#define NO_POCKET_GO             1
#define MIYOO3                1

#define LRAM_NUM              1
#define PALETTE_SIZE          256
#define DRIVER_NAME           "miyoofb"
#define SLCD_RESET		        ((32 * 4) + 11) // PE11
#define SLCD_TE		  		      ((32 * 4) + 10) // PE10

struct myfb_par {
  struct device *dev;
  struct platform_device *pdev;

  resource_size_t p_palette_base;
  unsigned short *v_palette_base;
 
  dma_addr_t vram_phys;
  uint32_t vram_size;
  void *vram_virt;
  int yoffset;
  
  dma_addr_t lram_phys[LRAM_NUM];
  uint32_t lram_size;
  void *lram_virt[LRAM_NUM];
  void *dma_addr;
 
  int bpp;
  int lcdc_irq;
  int gpio_irq;
  volatile int have_te;
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
};
static struct suniv_iomm iomm={0};

static int major = -1;
static struct cdev mycdev;
static struct class *myclass = NULL;
static int flip_mode=0;
static int new_bp=8;
static int new_fp=8;
static int def_bp=8;
static int def_fp=8;
uint32_t ret;
int x, y;
uint16_t *p;

struct myfb_par *mypar;
static struct fb_var_screeninfo myfb_var;
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

static void suniv_gpio_init(void)
{
  uint32_t ret;

  ret = readl(iomm.gpio + PD_CFG0);
  ret&= 0x0000000f;
  ret|= 0x22222220;
  writel(ret, iomm.gpio + PD_CFG0);
  writel(0x22222202, iomm.gpio + PD_CFG1);
  writel(0x00222222, iomm.gpio + PD_CFG2);
  writel(0x00040001, iomm.gpio + PD_PUL0);
  writel(0x00000000, iomm.gpio + PD_PUL1);
  //writel(0xffffffff, iomm.gpio + PD_DRV0);
  writel(0xffffffff, iomm.gpio + PD_DRV1);

  ret = readl(iomm.gpio + PE_PUL0);
  ret&= 0xff0fffff;
  ret|= 0x00500000;
  writel(ret, iomm.gpio + PE_PUL0);
  
  ret = readl(iomm.gpio + PE_CFG1);
  ret&= 0xffff0fff;
  ret|= 0x00001000;
  writel(ret, iomm.gpio + PE_CFG1);
}

static uint32_t lcdc_wait_busy(void)
{
	uint32_t cnt=0;
	
  suniv_setbits(iomm.lcdc + TCON0_CPU_IF_REG, (1 << 0));
	ndelay(10);
	while(1){
		if(readl(iomm.lcdc + TCON0_CPU_IF_REG) & 0x00c00000){
			if(cnt > 200){
				return -1;
			}
		  else{
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
	while(lcdc_wait_busy());
	if(is_data){
		suniv_setbits(iomm.lcdc + TCON0_CPU_IF_REG, (1 << 25)); // ca=1
	}
	else{
		suniv_clrbits(iomm.lcdc + TCON0_CPU_IF_REG, (1 << 25)); // ca=0	
	}
	while(lcdc_wait_busy());
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


static void refresh_lcd(struct myfb_par *par)
{

    lcdc_wr_cmd(0x22);

	if(par->yoffset == 0) {
		suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
		suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 9));
		suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 10));
		suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 11));
	}
	else if(par->yoffset == 240) {
		suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
		suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 9));
		suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 10));
		suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 11));
	}
	else if(par->yoffset == 480) {
		suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
		suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 9));
		suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 10));
		suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 11));
	}
	else if(par->yoffset == 720) {
		suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
		suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 9));
		suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 10));
		suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 11));
	} 
	suniv_setbits(iomm.debe + DEBE_REGBUFF_CTRL_REG, (1 << 0));
}

static irqreturn_t gpio_irq_handler(int irq, void *arg)
{
  static uint32_t cnt=0, report=0;
  struct myfb_par *par = arg;

  suniv_setbits(iomm.gpio + PE_INT_STA, (1 << 10));
  if(cnt > 10){
    par->have_te = 1;
    if(flip_mode == 0){
      if(report++ >= 60){
        report = 0;
        printk("%s\n", __func__);
      }
      refresh_lcd(par);
    }
  }
  else{
    cnt+= 1;
  }
  return IRQ_HANDLED;
}

static irqreturn_t lcdc_irq_handler(int irq, void *arg)
{
  static uint32_t report=0;
  struct myfb_par *par = arg;

  suniv_clrbits(iomm.lcdc + TCON_INT_REG0, (1 << 15));
  if(par->have_te == 0){
    if(flip_mode == 0){
      if(report++ >= 60){
        report = 0;
        //printk("%s\n", __func__);
      }
      refresh_lcd(par);
    }
  }
  return IRQ_HANDLED;
}

static void panel_init(void)
{
  
  suniv_setbits(iomm.lcdc + PE_DATA, (11 << 0));
  mdelay(50);
  suniv_clrbits(iomm.lcdc + PE_DATA, (11 << 0));
  mdelay(150);
  suniv_setbits(iomm.lcdc + PE_DATA, (11 << 0));
  mdelay(150);

  lcdc_wr_cmd(0x01); 
  lcdc_wr_dat(0x0100); // Set SS and SM bit
  lcdc_wr_cmd(0x02);
  lcdc_wr_dat(0x0700); // Set line inversion
  lcdc_wr_cmd(0x03);
  lcdc_wr_dat(0x1008); // Set Write direction
  lcdc_wr_cmd(0x04);
  lcdc_wr_dat(0x0000); // Set Scaling function off
  lcdc_wr_cmd(0x08); 
  lcdc_wr_dat(0x0207); // Set BP and FP
  lcdc_wr_cmd(0x09); 
  lcdc_wr_dat(0x0000); // Set non-display area
  lcdc_wr_cmd(0x0A); 
  lcdc_wr_dat(0x0000); // Frame marker control
  lcdc_wr_cmd(0x0C); 
  lcdc_wr_dat(0x0000); // Set interface control
  lcdc_wr_cmd(0x0D); 
  lcdc_wr_dat(0x0000); // Frame marker Position
  lcdc_wr_cmd(0x0F); 
  lcdc_wr_dat(0x0000); // Set RGB interface 
  //--------------- Power On Sequence----------------//
  lcdc_wr_cmd(0x10); 
  lcdc_wr_dat(0x0000); // Set SAP);BT[3:0]);AP);SLP);STB
  lcdc_wr_cmd(0x11); 
  lcdc_wr_dat(0x0007); // Set DC1[2:0]);DC0[2:0]);VC
  lcdc_wr_cmd(0x12); 
  lcdc_wr_dat(0x0000); // Set VREG1OUT voltage
  lcdc_wr_cmd(0x13); 
  lcdc_wr_dat(0x0000); // Set VCOM AMP voltage
  lcdc_wr_cmd(0x07); 
  lcdc_wr_dat(0x0001); // Set VCOM AMP voltage
  lcdc_wr_cmd(0x07); 
  lcdc_wr_dat(0x0020); // Set VCOM AMP voltage
  mdelay(200);
  lcdc_wr_cmd(0x10); 
  lcdc_wr_dat(0x1290); // Set SAP);BT[3:0]);AP);SLP);STB
  lcdc_wr_cmd(0x11); 
  lcdc_wr_dat(0x0221); // Set DC1[2:0]);DC0[2:0]);VC[2:0]
  mdelay(50);
  lcdc_wr_cmd(0x12); 
  lcdc_wr_dat(0x0081); // Set VREG1OUT voltaged
  mdelay(50);
  lcdc_wr_cmd(0x13); 
  lcdc_wr_dat(0x1500); // Set VCOM AMP voltage
  lcdc_wr_cmd(0x29); 
  lcdc_wr_dat(0x000c); // Set VCOMH voltage
  lcdc_wr_cmd(0x2B); 
  lcdc_wr_dat(0x000D); // Set Frame rate.
  mdelay(50);
  lcdc_wr_cmd(0x20); 
  lcdc_wr_dat(0x0000); // Set GRAM Horizontal Address
  lcdc_wr_cmd(0x21); 
  lcdc_wr_dat(0x0000); // Set GRAM Vertical Address
  //****************************************************
  lcdc_wr_cmd(0x30); 
  lcdc_wr_dat(0x0303);
  lcdc_wr_cmd(0x31); 
  lcdc_wr_dat(0x0006);
  lcdc_wr_cmd(0x32); 
  lcdc_wr_dat(0x0001);
  lcdc_wr_cmd(0x35); 
  lcdc_wr_dat(0x0204);
  lcdc_wr_cmd(0x36); 
  lcdc_wr_dat(0x0004);
  lcdc_wr_cmd(0x37); 
  lcdc_wr_dat(0x0407);
  lcdc_wr_cmd(0x38); 
  lcdc_wr_dat(0x0000);
  lcdc_wr_cmd(0x39); 
  lcdc_wr_dat(0x0404);
  lcdc_wr_cmd(0x3C); 
  lcdc_wr_dat(0x0402);
  lcdc_wr_cmd(0x3D); 
  lcdc_wr_dat(0x0004);
  //---------------  RAM Address Control ----------------//
  lcdc_wr_cmd(0x50); 
  lcdc_wr_dat(0x0000); // Set GRAM Horizontal Start Address
  lcdc_wr_cmd(0x51); 
  lcdc_wr_dat(0x00EF); // Set GRAM Horizontal End Address
  lcdc_wr_cmd(0x52); 
  lcdc_wr_dat(0x0000); // Set GRAM Vertical Start Address
  lcdc_wr_cmd(0x53); 
  lcdc_wr_dat(0x013F); // Set GRAM Vertical End Address
  //---------------  Panel Image Control -----------------//
  lcdc_wr_cmd(0x60); 
  lcdc_wr_dat(0x2700); // Set Gate Scan line
  lcdc_wr_cmd(0x61); 
  lcdc_wr_dat(0x0001); // Set NDL); VLE); REV
  lcdc_wr_cmd(0x6A); 
  lcdc_wr_dat(0x0000); // Set Scrolling line
  //---------------  Panel Interface Control---------------//
  lcdc_wr_cmd(0x90); 
  lcdc_wr_dat(0x0010);
  lcdc_wr_cmd(0x92); 
  lcdc_wr_dat(0x0000);
  //--------------- Display On-------------------------------//
  lcdc_wr_cmd(0x07); 
  lcdc_wr_dat(0x0133); // Display on
  lcdc_wr_cmd(0x22);


  writel(0xffffffff, iomm.gpio + PD_DATA);
  ret = readl(iomm.gpio + PD_CFG0);
  ret&= 0x0000000f;
  ret|= 0x22222220;
  writel(ret, iomm.gpio + PD_CFG0);
  writel(0x22222202, iomm.gpio + PD_CFG1);
  writel(0x00222222, iomm.gpio + PD_CFG2);

}

static void suniv_enable_irq(struct myfb_par *par)
{
  int ret;

  par->have_te = 0;
  par->lcdc_irq = platform_get_irq(par->pdev, 0);
  if (par->lcdc_irq < 0) {
    printk("%s, failed to get irq number for lcdc irq\n", __func__);
  }
  else{
	  ret = request_irq(par->lcdc_irq, lcdc_irq_handler, IRQF_SHARED, "miyoo_lcdc_irq", par);
	  if(ret){
		  printk("%s, failed to register lcdc interrupt(%d)\n", __func__, par->lcdc_irq);
	  }
  }
  
  par->gpio_irq = -1;//gpio_to_irq(SLCD_TE);
  if (par->gpio_irq < 0) {
    //printk("%s, failed to get irq number for gpio irq\n", __func__);
  }
  else{
	  ret = request_irq(par->gpio_irq, gpio_irq_handler, IRQF_TRIGGER_RISING, "miyoo_gpio_irq", par);
	  if(ret){
		  printk("%s, failed to register gpio interrupt(%d)\n", __func__, par->gpio_irq);
	  }
  }
}

static void suniv_lcdc_init(struct myfb_par *par)
{
	uint32_t ret=0, bp=0, total=0;
	uint32_t h_front_porch = 8;
	uint32_t h_back_porch = 8;
	uint32_t h_sync_len = 1;
	uint32_t v_front_porch = 8;
	uint32_t v_back_porch = 8;
	uint32_t v_sync_len = 1;

	writel(0, iomm.lcdc + TCON_CTRL_REG);
	writel(0, iomm.lcdc + TCON_INT_REG0);
	ret = readl(iomm.lcdc + TCON_CLK_CTRL_REG);
	ret&= ~(0xf << 28);
	writel(ret, iomm.lcdc + TCON_CLK_CTRL_REG);
	writel(0xffffffff, iomm.lcdc + TCON0_IO_CTRL_REG1);
	writel(0xffffffff, iomm.lcdc + TCON1_IO_CTRL_REG1);

	suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 0));
	writel(par->mode.xres << 4, iomm.debe + DEBE_LAY0_LINEWIDTH_REG);
	writel(par->mode.xres << 4, iomm.debe + DEBE_LAY1_LINEWIDTH_REG);
	writel(par->mode.xres << 4, iomm.debe + DEBE_LAY2_LINEWIDTH_REG);
	writel(par->mode.xres << 4, iomm.debe + DEBE_LAY3_LINEWIDTH_REG);
	writel((((par->mode.yres) - 1) << 16) | (((par->mode.xres) - 1) << 0), iomm.debe + DEBE_DISP_SIZE_REG);
	writel((((par->mode.yres) - 1) << 16) | (((par->mode.xres) - 1) << 0), iomm.debe + DEBE_LAY0_SIZE_REG);
	writel((((par->mode.yres) - 1) << 16) | (((par->mode.xres) - 1) << 0), iomm.debe + DEBE_LAY1_SIZE_REG);
	writel((((par->mode.yres) - 1) << 16) | (((par->mode.xres) - 1) << 0), iomm.debe + DEBE_LAY2_SIZE_REG);
	writel((((par->mode.yres) - 1) << 16) | (((par->mode.xres) - 1) << 0), iomm.debe + DEBE_LAY3_SIZE_REG);
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

	writel((uint32_t)(par->vram_phys + 320*240*2*0) << 3, iomm.debe + DEBE_LAY0_FB_ADDR_REG);
	writel((uint32_t)(par->vram_phys + 320*240*2*1) << 3, iomm.debe + DEBE_LAY1_FB_ADDR_REG);
	writel((uint32_t)(par->vram_phys + 320*240*2*2) << 3, iomm.debe + DEBE_LAY2_FB_ADDR_REG);
	writel((uint32_t)(par->vram_phys + 320*240*2*3) << 3, iomm.debe + DEBE_LAY3_FB_ADDR_REG);

	writel((uint32_t)(par->vram_phys + 320*240*2*0) >> 29, iomm.debe + DEBE_LAY0_FB_HI_ADDR_REG);
	writel((uint32_t)(par->vram_phys + 320*240*2*1) >> 29, iomm.debe + DEBE_LAY1_FB_HI_ADDR_REG);
	writel((uint32_t)(par->vram_phys + 320*240*2*2) >> 29, iomm.debe + DEBE_LAY2_FB_HI_ADDR_REG);
	writel((uint32_t)(par->vram_phys + 320*240*2*3) >> 29, iomm.debe + DEBE_LAY3_FB_HI_ADDR_REG);	
	  
	writel((1 << 31) | ((ret & 0x1f) << 4) | (1 << 24), iomm.lcdc + TCON0_CTRL_REG);
	  writel((0xf << 28) | (6 << 0), iomm.lcdc + TCON_CLK_CTRL_REG); // 6, 15
	  writel((4 << 29) | (1 << 26), iomm.lcdc + TCON0_CPU_IF_REG);
	  writel((1 << 28), iomm.lcdc + TCON0_IO_CTRL_REG0);


	writel(((par->mode.xres - 1) << 16) | ((par->mode.yres - 1) << 0), iomm.lcdc + TCON0_BASIC_TIMING_REG0);
	bp = h_sync_len + h_back_porch;
	total = par->mode.xres * 1 + h_front_porch + bp;
	writel(((total - 1) << 16) | ((bp - 1) << 0), iomm.lcdc + TCON0_BASIC_TIMING_REG1);
	bp = v_sync_len + v_back_porch;
	total = par->mode.yres + v_front_porch + bp;
	writel(((total * 2) << 16) | ((bp - 1) << 0), iomm.lcdc + TCON0_BASIC_TIMING_REG2);
	writel(((h_sync_len - 1) << 16) | ((v_sync_len - 1) << 0), iomm.lcdc + TCON0_BASIC_TIMING_REG3);
	writel(0, iomm.lcdc + TCON0_HV_TIMING_REG);
	writel(0, iomm.lcdc + TCON0_IO_CTRL_REG1);	

	suniv_setbits(iomm.lcdc + TCON_CTRL_REG, (1 << 31));
	suniv_setbits(iomm.gpio + PE_INT_STA, (1 << 10));
	suniv_setbits(iomm.lcdc + TCON_INT_REG0, (1 << 31));
	suniv_setbits(iomm.lcdc + TCON0_CPU_IF_REG, (1 << 28));
}

static void suniv_cpu_init(struct myfb_par *par)
{
  uint32_t ret, i;
    writel(0x91001307, iomm.ccm + PLL_VIDEO_CTRL_REG);
    while((readl(iomm.ccm + PLL_VIDEO_CTRL_REG) & (1 << 28)) == 0){}
    while((readl(iomm.ccm + PLL_PERIPH_CTRL_REG) & (1 << 28)) == 0){}

	ret = readl(iomm.ccm + DRAM_GATING_REG);
	ret|= (1 << 26) | (1 << 24);
	writel(ret, iomm.ccm + DRAM_GATING_REG);
  
  suniv_setbits(iomm.ccm + FE_CLK_REG, (1 << 31));
  suniv_setbits(iomm.ccm + BE_CLK_REG, (1 << 31));
  suniv_setbits(iomm.ccm + TCON_CLK_REG, (1 << 31));
  suniv_setbits(iomm.ccm + BUS_CLK_GATING_REG1, (1 << 14) | (1 << 12) | (1 << 4));
  suniv_setbits(iomm.ccm + BUS_SOFT_RST_REG1, (1 << 14) | (1 << 12) | (1 << 4));
  for(i=0x0800; i<0x1000; i+=4){
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
 
  if((var->xres != 320) || (var->yres != 240) || (var->bits_per_pixel != 16)){
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
  if(line_size * var->yres_virtual > par->vram_size){
    var->yres_virtual = par->vram_size / line_size;
  }
  if(var->yres > var->yres_virtual){
    var->yres = var->yres_virtual;
  }
  if(var->xres > var->xres_virtual){
    var->xres = var->xres_virtual;
  }
  if(var->xres + var->xoffset > var->xres_virtual){
    var->xoffset = var->xres_virtual - var->xres;
  }
  if(var->yres + var->yoffset > var->yres_virtual){
    var->yoffset = var->yres_virtual - var->yres;
  }
  return 0;
}
 
static int myfb_remove(struct platform_device *dev)
{
  int i;
  struct fb_info *info = dev_get_drvdata(&dev->dev);
  struct myfb_par *par = info->par;
 
  if(info){
    free_irq(par->lcdc_irq, par);
    free_irq(par->gpio_irq, par);
    flush_scheduled_work();
    unregister_framebuffer(info);
    fb_dealloc_cmap(&info->cmap);
    dma_free_coherent(NULL, PALETTE_SIZE, par->v_palette_base, par->p_palette_base);
    dma_free_coherent(NULL, par->vram_size, par->vram_virt, par->vram_phys);
    for(i=0; i<LRAM_NUM; i++){
      dma_free_coherent(NULL, par->lram_size, par->lram_virt[i], par->lram_phys[i]);
    }
    pm_runtime_put_sync(&dev->dev);
    pm_runtime_disable(&dev->dev);
    framebuffer_release(info);
  }
  return 0;
}
 
static int myfb_set_par(struct fb_info *info)
{
  struct myfb_par *par = info->par;
   
  fb_var_to_videomode(&par->mode, &info->var);
  par->yoffset = info->var.yoffset;
  par->bpp = info->var.bits_per_pixel;
  info->fix.visual = FB_VISUAL_TRUECOLOR;
  info->fix.line_length = (par->mode.xres * par->bpp) / 8; 	
  writel((5 << 8), iomm.debe + DEBE_LAY0_ATT_CTRL_REG1);
  writel((5 << 8), iomm.debe + DEBE_LAY1_ATT_CTRL_REG1);
  return 0;
}
 
static int myfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
  switch(cmd){
  case FBIO_WAITFORVSYNC:
    break;
  }
  return 0;
}
 
static int myfb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
  const unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
  const unsigned long size = vma->vm_end - vma->vm_start;
  
  if(offset + size > info->fix.smem_len){
    return -EINVAL;
  }
  
  if(remap_pfn_range(vma, vma->vm_start, (info->fix.smem_start + offset) >> PAGE_SHIFT, size, vma->vm_page_prot)){
    return -EAGAIN;
  }
  return 0;
}

static int myfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
  struct myfb_par *par = info->par;
  if((var->xoffset != info->var.xoffset) || (var->yoffset != info->var.yoffset)){
    info->var.xoffset = var->xoffset;
    info->var.yoffset = var->yoffset;
    par->yoffset = var->yoffset;
  }
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
  int i, ret;
  struct fb_info *info;
  struct myfb_par *par;
  struct fb_videomode *mode;

  mode = devm_kzalloc(&device->dev, sizeof(struct fb_videomode), GFP_KERNEL);
  if(mode == NULL){
    return -ENOMEM;
  }
  mode->name = "320x240";
  mode->xres = 320;
  mode->yres = 240;
  mode->vmode = FB_VMODE_NONINTERLACED; 
  pm_runtime_enable(&device->dev);
  pm_runtime_get_sync(&device->dev);
   
  info = framebuffer_alloc(sizeof(struct myfb_par), &device->dev);
  if(!info){
    dev_dbg(&device->dev, "memory allocation failed for fb_info\n");
    return -ENOMEM;
  }
 
  par = info->par;
  par->pdev = device;
  par->dev = &device->dev;
  par->bpp = 16;
  fb_videomode_to_var(&myfb_var, mode);
  
  par->vram_size = 320*240*2*2;
  par->vram_virt = dma_alloc_coherent(par->dev, par->vram_size, (resource_size_t*)&par->vram_phys, GFP_KERNEL | GFP_DMA);
  if(!par->vram_virt){
    dev_err(&device->dev, "%s, failed to allocate frame buffer(vram)\n", __func__);
    return -EINVAL;
  }
  info->screen_base = (char __iomem*)par->vram_virt;
  myfb_fix.smem_start = par->vram_phys;
  myfb_fix.smem_len = par->vram_size;
  myfb_fix.line_length = 320 * 2;
  
  par->lram_size = 320*240*18;
  for(i=0; i<LRAM_NUM; i++){
    par->lram_virt[i] = dma_alloc_coherent(par->dev, par->lram_size, (resource_size_t*)&par->lram_phys[i], GFP_KERNEL | GFP_DMA);
    if(!par->lram_virt[i]){
      dev_err(&device->dev, "%s, failed to allocate frame buffer(lram[%d])\n", __func__, i);
      return -EINVAL;
    }
  }
   
  par->v_palette_base = dma_alloc_coherent(par->dev, PALETTE_SIZE, (resource_size_t*)&par->p_palette_base, GFP_KERNEL | GFP_DMA);
  if(!par->v_palette_base){
    dev_err(&device->dev, "GLCD: kmalloc for palette buffer failed\n");
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
  if(ret){
    return -EINVAL;
  }
  info->cmap.len = 32;
 
  myfb_var.activate = FB_ACTIVATE_FORCE;
  fb_set_var(info, &myfb_var);
  dev_set_drvdata(&device->dev, info);

  if(register_framebuffer(info) < 0){
    dev_err(&device->dev, "failed to register /dev/fb0\n");
    return -EINVAL;
  }
  
  mypar = par;
  new_bp = -1;
  new_fp = -1;
  for(ret=0; ret<of_clk_get_parent_count(device->dev.of_node); ret++){
    clk_prepare_enable(of_clk_get(device->dev.of_node, ret));
  }
  suniv_cpu_init(par);
  suniv_gpio_init();
  suniv_lcdc_init(par);
  suniv_enable_irq(par);
  
  fb_prepare_logo(info, 0);
  fb_show_logo(info, 0);
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
  iomm.dma = (uint8_t*)ioremap(SUNIV_DMA_BASE, 4096);
  iomm.ccm = (uint8_t*)ioremap(SUNIV_CCM_BASE, 4096);
  iomm.gpio = (uint8_t*)ioremap(SUNIV_GPIO_BASE, 4096);
  iomm.lcdc = (uint8_t*)ioremap(SUNIV_LCDC_BASE, 4096);
  iomm.debe = (uint8_t*)ioremap(SUNIV_DEBE_BASE, 4096);
  iomm.intc = (uint8_t*)ioremap(SUNIV_INTC_BASE, 4096);
}

static void suniv_iounmap(void)
{
  iounmap(iomm.dma);
  iounmap(iomm.ccm);
  iounmap(iomm.gpio);
  iounmap(iomm.lcdc);
  iounmap(iomm.debe);
  iounmap(iomm.intc);
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
  int32_t w, bpp;
  unsigned long tmp;

  switch(cmd){
  case MIYOO_FB0_PUT_OSD:
    break;
  case MIYOO_FB0_SET_MODE:
    w = (arg >> 16);
    bpp = (arg & 0xffff);
    if((bpp != 16)){
	    writel((5 << 8), iomm.debe + DEBE_LAY0_ATT_CTRL_REG1);
      writel((5 << 8), iomm.debe + DEBE_LAY1_ATT_CTRL_REG1);
    }
    else{
	    writel((7 << 8) | 4, iomm.debe + DEBE_LAY0_ATT_CTRL_REG1);
	    writel((7 << 8) | 4, iomm.debe + DEBE_LAY1_ATT_CTRL_REG1);
    }
    break;
  case MIYOO_FB0_GET_VER:
    break;
  case MIYOO_FB0_SET_FLIP:
    break;
  case MIYOO_FB0_GET_FPBP:
    tmp = (def_fp & 0x0f) << 12;
    tmp|= (def_bp & 0x0f) <<  8;
    tmp|= (new_fp & 0x0f) <<  4;
    tmp|= (new_bp & 0x0f) <<  0;
    w = copy_to_user((void*)arg, &tmp, sizeof(unsigned long));
    break;
  case MIYOO_FB0_SET_FPBP:
    new_fp = (arg & 0xf0) >> 4;
    new_bp = (arg & 0x0f) >> 0;
	  
    flip_mode = 1;
    mdelay(100);
    panel_init();
    flip_mode = 0;
	  suniv_setbits(iomm.debe + DEBE_REGBUFF_CTRL_REG, (1 << 0));
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
 
MODULE_DESCRIPTION("Allwinner suniv framebuffer driver for Miyoo handheld");
MODULE_AUTHOR("Steward Fu <steward.fu@gmail.com>");
MODULE_LICENSE("GPL");
