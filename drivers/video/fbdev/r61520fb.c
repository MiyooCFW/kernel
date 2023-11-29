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

//#define DEBUG
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

static bool flip=false;
module_param(flip,bool,0660);

static bool debug=false;
module_param(debug,bool,0660);

static bool lowcurrent=false;
module_param(lowcurrent,bool,0660);

static uint32_t version=0;
module_param(version,uint,0660);

static bool invert=false;
module_param(invert,bool,0660);

// Which LCD controller are we driving? E.g. R61520, R61526, R61581, etc. 
// Should be a number 1-4, but I'm not yet sure which numbers correspond to which panels
// 1 = R61520 - R61520 doesn't support the 0x04 read_DDB_start / Read Display ID Information command.
// 2 = ST7789S
// 3 = R61505W
// 4 = ?
// Will be automatically detected if left unset (which usually works)

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
static uint32_t miyoo_ver=0;
static int flip_mode=0;
static int new_bp=8;
static int new_fp=8;
static int def_bp=8;
static int def_fp=8;
uint32_t ret;
int x, y;
uint16_t *p;
uint32_t ddr_clock;

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
    ret = readl(iomm.gpio + PD_CFG1);
    ret&= 0x000000f0;
    ret|= 0x22222212;
    writel(ret, iomm.gpio + PD_CFG1);
  writel(0x00222222, iomm.gpio + PD_CFG2);
  writel(0x00040001, iomm.gpio + PD_PUL0);
  writel(0x00000000, iomm.gpio + PD_PUL1);
  //writel(0xffffffff, iomm.gpio + PD_DRV0);
  writel(0xffffffff, iomm.gpio + PD_DRV1);

  if (lowcurrent){
    writel(0x00000000, iomm.gpio + PD_DRV0);
  }

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

static uint32_t extend_24b_to_16b(uint32_t value)
{
	return ((value & 0xfc0000) >> 8) | ((value & 0xc000) >> 6) | ((value & 0x1c00) >> 5) | ((value & 0x00f8) >> 3);
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

static uint32_t lcdc_rd_dat(void)
{
	while(lcdc_wait_busy());
	suniv_setbits(iomm.lcdc + TCON0_CPU_IF_REG, (1 << 25)); // ca=1
	while(lcdc_wait_busy());
	return extend_24b_to_16b(readl(iomm.lcdc + TCON0_CPU_RD_REG));
}

static void gpio_wr(uint32_t is_data, uint32_t val)
{
  uint32_t ret;

  ret = (val & 0x00ff) << 1;
  ret|= (val & 0xff00) << 2;
  ret|= is_data ? 0x80000 : 0;
  ret|= 0x100000; 
  writel(ret, iomm.gpio + PD_DATA);
  ret|= 0x40000;
  writel(ret, iomm.gpio + PD_DATA);
}

static void ser_wr(uint8_t is_data, uint8_t data)
{
  int bit;
 
  suniv_clrbits(iomm.gpio + PC_DATA, 1 << 0); // csx
	if(is_data){
    suniv_setbits(iomm.gpio + PE_DATA, 1 << 0); // wrx
	}
	else{
    suniv_clrbits(iomm.gpio + PE_DATA, 1 << 0); // wrx
	}
  for(bit=7; bit>=0; bit--){
  	suniv_clrbits(iomm.gpio + PC_DATA, 1 << 3); // dcx
		if((data >> bit) & 1){
			suniv_setbits(iomm.gpio + PE_DATA, 1 << 1); // sda
		}
		else{
			suniv_clrbits(iomm.gpio + PE_DATA, 1 << 1); // sda
		}
    udelay(10);
  	suniv_setbits(iomm.gpio + PC_DATA, 1 << 3); // dcx
    udelay(10);
  }
}

static void ser_wr_cmd(uint8_t data)
{
  ser_wr(0, data);
}
 
static void ser_wr_dat(uint8_t data)
{
  ser_wr(1, data);
}

static uint32_t ser_rd_dat(void)
{
  int bit;
	uint32_t ret=0;
 
  suniv_clrbits(iomm.gpio + PC_DATA, 1 << 0); // csx
	suniv_clrbits(iomm.gpio + PE_CFG0, 1 << 4); // sda
  suniv_setbits(iomm.gpio + PE_DATA, 1 << 0); // wrx
  for(bit=31; bit>=0; bit--){
    udelay(10);
  	suniv_clrbits(iomm.gpio + PC_DATA, 1 << 3); // dcx
		ret<<= 1;
		if(readl(iomm.gpio + PE_DATA) & 2){ // sda
			ret|= 1;
		}
    udelay(10);
  	suniv_setbits(iomm.gpio + PC_DATA, 1 << 3); // dcx
  }
	suniv_setbits(iomm.gpio + PE_CFG0, 1 << 4); // sda
	return ret;
}

static void ser_init(void)
{
  suniv_setbits(iomm.gpio + PC_CFG0, 1 << 0); // csx
  suniv_clrbits(iomm.gpio + PC_CFG0, 1 << 1);
  suniv_clrbits(iomm.gpio + PC_CFG0, 1 << 2);
  suniv_setbits(iomm.gpio + PC_DATA, 1 << 0);

  suniv_setbits(iomm.gpio + PC_CFG0, 1 << 12); // dcx
  suniv_clrbits(iomm.gpio + PC_CFG0, 1 << 13);
  suniv_clrbits(iomm.gpio + PC_CFG0, 1 << 14);
  suniv_setbits(iomm.gpio + PC_DATA, 1 << 3);

  suniv_setbits(iomm.gpio + PE_CFG0, 1 << 0); // wrx
  suniv_clrbits(iomm.gpio + PE_CFG0, 1 << 1);
  suniv_clrbits(iomm.gpio + PE_CFG0, 1 << 2);
  suniv_setbits(iomm.gpio + PE_DATA, 1 << 0);
  suniv_setbits(iomm.gpio + PE_PUL0, 1 << 0);
  suniv_clrbits(iomm.gpio + PE_PUL0, 1 << 1);

  suniv_setbits(iomm.gpio + PE_CFG0, 1 << 4); // sda
  suniv_clrbits(iomm.gpio + PE_CFG0, 1 << 5);
  suniv_clrbits(iomm.gpio + PE_CFG0, 1 << 6);
  suniv_setbits(iomm.gpio + PE_DATA, 1 << 1);
  suniv_setbits(iomm.gpio + PE_PUL0, 1 << 2);
  suniv_clrbits(iomm.gpio + PE_PUL0, 1 << 3);
}

static void ser_deinit(void)
{
  suniv_clrbits(iomm.gpio + PE_CFG0, 1 << 0);
  suniv_clrbits(iomm.gpio + PE_CFG0, 1 << 1);
  suniv_clrbits(iomm.gpio + PE_CFG0, 1 << 2);

  suniv_clrbits(iomm.gpio + PE_CFG0, 1 << 4);
  suniv_clrbits(iomm.gpio + PE_CFG0, 1 << 5);
  suniv_clrbits(iomm.gpio + PE_CFG0, 1 << 6);
}

static void gpio_wr_cmd(uint32_t val)
{
  gpio_wr(0, val);
}

static void gpio_wr_dat(uint32_t val)
{
  gpio_wr(1, val);
}

static void refresh_lcd(struct myfb_par *par)
{
  if((miyoo_ver <= 2) || (miyoo_ver == 4)){
    lcdc_wr_cmd(0x2c);
  }
  if(par->yoffset == 0){
	  suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
	  suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 9));
  }
  else{
	  suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
	  suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 9));
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


static void readReg(uint32_t reg, uint8_t n, const char *msg) //this is for debugging registers on TFT panels
{
  uint32_t res;
  lcdc_wr_cmd(reg);
  res = lcdc_rd_dat();
  printk("REG 0x%02x:     ", reg);
  for(x=0; x<n; x++){
    res = lcdc_rd_dat();
    printk(KERN_CONT "%02x ",res);
  }
  printk(KERN_CONT "%s ",msg);
}





static int panel_init(void)
{
  uint16_t x, ver[4]={0};
  
  suniv_setbits(iomm.lcdc + PE_DATA, (11 << 0));
  mdelay(50);
  suniv_clrbits(iomm.lcdc + PE_DATA, (11 << 0));
  mdelay(150);
  suniv_setbits(iomm.lcdc + PE_DATA, (11 << 0));
  mdelay(150);


  
if(debug){
    printk(" ");
    printk("TFT IDENTIFICATION 8BITS:");
    printk(" ");
    readReg(0x00, 4, "ID: ILI9320, ILI9325, ILI9335, ...");
    readReg(0x04, 4, "Manufacturer ID");
    readReg(0x09, 5, "Status Register");
    readReg(0x0A, 2, "Get Power Mode");
    readReg(0x0C, 2, "Get Pixel Format");
    readReg(0x30, 5, "PTLAR");
    readReg(0x33, 7, "VSCRLDEF");
    readReg(0x61, 2, "RDID1 HX8347-G");
    readReg(0x62, 2, "RDID2 HX8347-G");
    readReg(0x63, 2, "RDID3 HX8347-G");
    readReg(0x64, 2, "RDID1 HX8347-A");
    readReg(0x65, 2, "RDID2 HX8347-A");
    readReg(0x66, 2, "RDID3 HX8347-A");
    readReg(0x67, 2, "RDID Himax HX8347-A");
    readReg(0x70, 2, "Panel Himax HX8347-A");
    readReg(0xA1, 5, "RD_DDB SSD1963");
    readReg(0xB0, 2, "RGB Interface Signal Control");
    readReg(0xB3, 5, "Frame Memory");
    readReg(0xB4, 2, "Frame Mode");
    readReg(0xB6, 5, "Display Control");
    readReg(0xB7, 2, "Entry Mode Set");
    readReg(0xBF, 6, "ILI9481, HX8357-B");
    readReg(0xC0, 9, "Panel Control");
    readReg(0xC1, 4, "Display Timing");
    readReg(0xC5, 2, "Frame Rate");
    readReg(0xC8, 13, "GAMMA");
    readReg(0xCC, 2, "Panel Control");
    readReg(0xD0, 4, "Power Control");
    readReg(0xD1, 4, "VCOM Control");
    readReg(0xD2, 3, "Power Normal");
    readReg(0xD3, 4, "ILI9341, ILI9488");
    readReg(0xD4, 4, "Novatek");
    readReg(0xDA, 2, "RDID1");
    readReg(0xDB, 2, "RDID2");
    readReg(0xDC, 2, "RDID3");
    readReg(0xE0, 16, "GAMMA-P");
    readReg(0xE1, 16, "GAMMA-N");
    readReg(0xEF, 6, "ILI9327");
    readReg(0xF2, 12, "Adjust Control 2");
    readReg(0xF6, 4, "Interface Control");
  }

  suniv_setbits(iomm.lcdc + PE_DATA, (11 << 0));
  mdelay(50);
  suniv_clrbits(iomm.lcdc + PE_DATA, (11 << 0));
  mdelay(150);
  suniv_setbits(iomm.lcdc + PE_DATA, (11 << 0));
  mdelay(150);

  lcdc_wr_cmd(0x04);
  for(x=0; x<4; x++){
    ver[x] = lcdc_rd_dat();
  }
  if(ver[2]){
    miyoo_ver = 2;
  }
  else{
    lcdc_wr_cmd(0x00);
    for(x=0; x<4; x++){
      ver[x] = lcdc_rd_dat();
    }
    miyoo_ver = 3;
    if(ver[2] == 0){
      miyoo_ver = 1;
      ser_init();
      ser_wr_cmd(0x04);
      ret = ser_rd_dat();
      if(ret == 0x42c2a97f){
        miyoo_ver = 4;
      }
      ser_deinit();
    }
  }

  if (version && version != miyoo_ver) {
    printk("Warning: LCD controller detected as version %d, but being overridden by module_param as version %d (detection 2 of 2)", miyoo_ver, version);
    miyoo_ver = version;
  }

  ret = readl(iomm.gpio + PD_CFG0);
  ret&= 0x0000000f;
  ret|= 0x11111110;
  writel(ret, iomm.gpio + PD_CFG0);
  writel(0x11111101, iomm.gpio + PD_CFG1);
  writel(0x00111111, iomm.gpio + PD_CFG2);
  writel(0xffffffff, iomm.gpio + PD_DATA);
    mdelay(150);
  switch(miyoo_ver){
  case 1: // R61520
    gpio_wr_cmd(0xb0);
    gpio_wr_dat(0x00);

    gpio_wr_cmd(0xb1);
    gpio_wr_dat(0x00);

    gpio_wr_cmd(0xb3);
    gpio_wr_dat(0x02);
    gpio_wr_dat(0x00); // every frame
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x00);

    gpio_wr_cmd(0xb4);
    gpio_wr_dat(0x00);

    gpio_wr_cmd(0xc0);
    gpio_wr_dat(0x07);
    gpio_wr_dat(0x4f); // nl 320 lines
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x00); // 1 line
    gpio_wr_dat(0x00); // line inversion
    gpio_wr_dat(0x00); 
    gpio_wr_dat(0x01);
    gpio_wr_dat(0x33); // pcdiv 0x33

    gpio_wr_cmd(0xc1);
    gpio_wr_dat(0x01); // line inversion
    gpio_wr_dat(0x00); // division 0x00
    gpio_wr_dat(0x1a); // clock 0x1a
    if((new_fp == -1) || (new_bp == -1)){
      def_bp = new_bp = 0x08;
      def_fp = new_fp = 0x08;
    }
    gpio_wr_dat(new_bp); // bp0 0x08
    gpio_wr_dat(new_fp); // fp0 0x08

    gpio_wr_cmd(0xc3);
    gpio_wr_dat(0x01); // line inversion
    gpio_wr_dat(0x00); // division 0x00
    gpio_wr_dat(0x1a); // clock 0x1a
    gpio_wr_dat(new_bp); // bp2 0x0a
    gpio_wr_dat(new_fp); // fp2 0x09

    gpio_wr_cmd(0xc4);
    gpio_wr_dat(0x11);
    gpio_wr_dat(0x01);
    gpio_wr_dat(0x43);
    gpio_wr_dat(0x01);

    gpio_wr_cmd(0xc8);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x0a);
    gpio_wr_dat(0x08);
    gpio_wr_dat(0x8a);
    gpio_wr_dat(0x08);
    gpio_wr_dat(0x09);
    gpio_wr_dat(0x05);
    gpio_wr_dat(0x10);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x23);
    gpio_wr_dat(0x10);
    gpio_wr_dat(0x05);
    gpio_wr_dat(0x05);
    gpio_wr_dat(0x60);
    gpio_wr_dat(0x0a);
    gpio_wr_dat(0x08);
    gpio_wr_dat(0x05);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x10);
    gpio_wr_dat(0x00);

    gpio_wr_cmd(0xc9);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x0a);
    gpio_wr_dat(0x08);
    gpio_wr_dat(0x8a);
    gpio_wr_dat(0x08);
    gpio_wr_dat(0x09);
    gpio_wr_dat(0x05);
    gpio_wr_dat(0x10);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x23);
    gpio_wr_dat(0x10);
    gpio_wr_dat(0x05);
    gpio_wr_dat(0x09);
    gpio_wr_dat(0x88);
    gpio_wr_dat(0x0a);
    gpio_wr_dat(0x08);
    gpio_wr_dat(0x0a);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x23);
    gpio_wr_dat(0x00);

    gpio_wr_cmd(0xca);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x0a);
    gpio_wr_dat(0x08);
    gpio_wr_dat(0x8a);
    gpio_wr_dat(0x08);
    gpio_wr_dat(0x09);
    gpio_wr_dat(0x05);
    gpio_wr_dat(0x10);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x23);
    gpio_wr_dat(0x10);
    gpio_wr_dat(0x05);
    gpio_wr_dat(0x09);
    gpio_wr_dat(0x88);
    gpio_wr_dat(0x0a);
    gpio_wr_dat(0x08);
    gpio_wr_dat(0x0a);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x23);
    gpio_wr_dat(0x00);

    gpio_wr_cmd(0xd0);
    gpio_wr_dat(0x07);
    gpio_wr_dat(0xc6);
    gpio_wr_dat(0xdc);

    gpio_wr_cmd(0xd1);
    gpio_wr_dat(0x54);
    gpio_wr_dat(0x0d);
    gpio_wr_dat(0x02);

    gpio_wr_cmd(0xd2);
    gpio_wr_dat(0x63);
    gpio_wr_dat(0x24);

    gpio_wr_cmd(0xd4);
    gpio_wr_dat(0x63);
    gpio_wr_dat(0x24);

    gpio_wr_cmd(0xd8);
    gpio_wr_dat(0x07);
    gpio_wr_dat(0x07);

    gpio_wr_cmd(0xe0);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x00);

    gpio_wr_cmd(0x13);

    if (invert) {
      gpio_wr_cmd(0x21); // enter_invert_mode for colors
    } else {
      gpio_wr_cmd(0x20); // exit_invert_mode for colors
    }

    gpio_wr_cmd(0x35);
    gpio_wr_dat(0x00); // te mode

    gpio_wr_cmd(0x44);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x30);

    gpio_wr_cmd(0x36);
    if(flip){
        gpio_wr_dat(0x38);
    } else {
        gpio_wr_dat(0xe0);
    }

    gpio_wr_cmd(0x3a);
    gpio_wr_dat(0x55);

    gpio_wr_cmd(0x2a);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x01);
    gpio_wr_dat(0x3f);

    gpio_wr_cmd(0x2b);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0xef);
    
    gpio_wr_cmd(0x11);
    mdelay(150);
    gpio_wr_cmd(0x29);
    mdelay(150);
    gpio_wr_cmd(0x2c);
    break;
  case 2: // ST7789S
    gpio_wr_cmd(0x11);
    mdelay(250);
                  
    gpio_wr_cmd(0x36);
    if(flip){
    	gpio_wr_dat(0xB0); //screen direction //0x70 for 3.5, 0xB0 for pg
    } else {
    	gpio_wr_dat(0x70); //screen direction //0x70 for 3.5, 0xB0 for pg
    }
                          
    gpio_wr_cmd(0x3a);
    gpio_wr_dat(0x05);
      
    gpio_wr_cmd(0x2a);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x01);
    gpio_wr_dat(0x3f);

    gpio_wr_cmd(0x2b);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0xef);
        
    // ST7789S Frame rate setting
    gpio_wr_cmd(0xb2);
    if((new_fp == -1) || (new_bp == -1)){
      def_bp = new_bp = 9;
      def_fp = new_fp = 10;
    }
    // BP:10 and FP:4 or BP:13 FP:2 
    gpio_wr_dat(new_bp); // bp 0x0a
    gpio_wr_dat(new_fp); // fp 0x0b
    gpio_wr_dat(0x00);        			
    gpio_wr_dat(0x33);
    gpio_wr_dat(0x33);

    // Gate Control
    gpio_wr_cmd(0xb7);
    gpio_wr_dat(0x35);

    // ?
    gpio_wr_cmd(0xb8);
    gpio_wr_dat(0x2f);
    gpio_wr_dat(0x2b);
    gpio_wr_dat(0x2f);

    // ST7789S Power setting
    gpio_wr_cmd(0xbb);
    gpio_wr_dat(0x15);
          
    gpio_wr_cmd(0xc0);
    gpio_wr_dat(0x3C);
        
    gpio_wr_cmd(0xc2);
    gpio_wr_dat(0x01);							

    gpio_wr_cmd(0xc3);
    gpio_wr_dat(0x13); // or 0x0b?

    if (invert) {
      gpio_wr_cmd(0x21); // Display Inversion On (INVON for colors)
    } else {
      gpio_wr_cmd(0x20); //  Display Inversion Off (INVOFF for colors)
    }

    gpio_wr_cmd(0xc4);
    gpio_wr_dat(0x20);

    gpio_wr_cmd(0xc6);
    gpio_wr_dat(0x04); // 0x04, 0x1f

    gpio_wr_cmd(0xd0);
    gpio_wr_dat(0xa4);
    gpio_wr_dat(0xa1);
        
    gpio_wr_cmd(0xe8);
    gpio_wr_dat(0x03);

    gpio_wr_cmd(0xe9);
    gpio_wr_dat(0x0d);
    gpio_wr_dat(0x12);
    gpio_wr_dat(0x00);

    // ST7789S gamma setting
    gpio_wr_cmd(0xe0);
    gpio_wr_dat(0x70);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x06);
    gpio_wr_dat(0x09);
    gpio_wr_dat(0x0b);
    gpio_wr_dat(0x2a);
    gpio_wr_dat(0x3c);
    gpio_wr_dat(0x33);
    gpio_wr_dat(0x4b);
    gpio_wr_dat(0x08);
    gpio_wr_dat(0x16);
    gpio_wr_dat(0x14);
    gpio_wr_dat(0x2a);
    gpio_wr_dat(0x23);
        
    gpio_wr_cmd(0xe1);
    gpio_wr_dat(0xd0);
    gpio_wr_dat(0x00);
    gpio_wr_dat(0x06);
    gpio_wr_dat(0x09);
    gpio_wr_dat(0x0b);
    gpio_wr_dat(0x29);
    gpio_wr_dat(0x36);
    gpio_wr_dat(0x54);
    gpio_wr_dat(0x4b);
    gpio_wr_dat(0x0d);
    gpio_wr_dat(0x16);
    gpio_wr_dat(0x14);
    gpio_wr_dat(0x28);
    gpio_wr_dat(0x22);

    mdelay(50);
    gpio_wr_cmd(0x29);
    mdelay(50);
    gpio_wr_cmd(0x2c);
    mdelay(100);
    break;
  case 3:
    gpio_wr_cmd(0xa4);
    gpio_wr_dat(0x0001);
    mdelay(50);
    gpio_wr_cmd(0x9c);
    gpio_wr_dat(0x0033); // pcdiv
    gpio_wr_cmd(0x60);
    gpio_wr_dat(0x2700); // 320 lines
    gpio_wr_cmd(0x08);
    if((new_fp == -1) || (new_bp == -1)){
      def_bp = new_bp = 0x08;
      def_fp = new_fp = 0x08;
    }
    gpio_wr_dat((new_fp << 8)| new_bp); // fp, bp
    gpio_wr_cmd(0x30);
    gpio_wr_dat(0x0103); // gamma
    gpio_wr_cmd(0x31);
    gpio_wr_dat(0x1811);
    gpio_wr_cmd(0x32);
    gpio_wr_dat(0x0501);
    gpio_wr_cmd(0x33);
    gpio_wr_dat(0x0510);
    gpio_wr_cmd(0x34);
    gpio_wr_dat(0x2010);
    gpio_wr_cmd(0x35);
    gpio_wr_dat(0x1005);
    gpio_wr_cmd(0x36);
    gpio_wr_dat(0x1105);
    gpio_wr_cmd(0x37);
    gpio_wr_dat(0x1109);
    gpio_wr_cmd(0x38);
    gpio_wr_dat(0x0301);
    gpio_wr_cmd(0x39);
    gpio_wr_dat(0x1020);
    gpio_wr_cmd(0x90);
    gpio_wr_dat(0x001f); // 80hz, 0x0016, 0x1f
    gpio_wr_cmd(0x10);
    gpio_wr_dat(0x0530); // bt, ap
    gpio_wr_cmd(0x11);
    gpio_wr_dat(0x0247); // dc1, dc0, vc
    gpio_wr_cmd(0x12);
    gpio_wr_dat(0x01bc);
    gpio_wr_cmd(0x13);
    gpio_wr_dat(0x1000);
    mdelay(50);
    gpio_wr_cmd(0x01);
    gpio_wr_dat(0x0100);
    gpio_wr_cmd(0x02);
    gpio_wr_dat(0x0200);
    gpio_wr_cmd(0x03);
    gpio_wr_dat(0x1028); // 0x1028
    gpio_wr_cmd(0x09);
    gpio_wr_dat(0x0001);
    gpio_wr_cmd(0x0a);
    gpio_wr_dat(0x0008); // one frame
    gpio_wr_cmd(0x0c);
    gpio_wr_dat(0x0000);
    gpio_wr_cmd(0x0d);
    gpio_wr_dat(0xd000); // frame mark 0xd000
    gpio_wr_cmd(0x0e);
    gpio_wr_dat(0x0030);
    gpio_wr_cmd(0x0f);
    gpio_wr_dat(0x0000);
    gpio_wr_cmd(0x20);
    gpio_wr_dat(0x0000); // H Start
    gpio_wr_cmd(0x21);
    gpio_wr_dat(0x0000); // V Start
    gpio_wr_cmd(0x29);
    gpio_wr_dat(0x002e);
    gpio_wr_cmd(0x50);
    gpio_wr_dat(0x0000);
    gpio_wr_cmd(0x51);
    gpio_wr_dat(0xd0ef);
    gpio_wr_cmd(0x52);
    gpio_wr_dat(0x0000);
    gpio_wr_cmd(0x53);
    gpio_wr_dat(0x013f);
    gpio_wr_cmd(0x61);
    gpio_wr_dat(0x0000);
    gpio_wr_cmd(0x6a);
    gpio_wr_dat(0x0000);
    gpio_wr_cmd(0x80);
    gpio_wr_dat(0x0000);
    gpio_wr_cmd(0x81);
    gpio_wr_dat(0x0000);
    gpio_wr_cmd(0x82);
    gpio_wr_dat(0x005f);
    gpio_wr_cmd(0x93);
    gpio_wr_dat(0x0507);
    gpio_wr_cmd(0x07);
    gpio_wr_dat(0x0100);
    mdelay(150);
    gpio_wr_cmd(0x22);
    mdelay(150);
    break;
  case 4:
    ser_init();
  #if 0
		ser_wr_cmd(0x36);
		ser_wr_dat(0x80);
	 
		ser_wr_cmd(0xb2); // porch
		ser_wr_dat(0x08); // bpa
		ser_wr_dat(0x08); // fpa
		ser_wr_dat(0x00); // psen
		ser_wr_dat(0x88); // bpb
		ser_wr_dat(0x88); // bpc
	 
		ser_wr_cmd(0xb7);
		ser_wr_dat(0x35);
	 
		ser_wr_cmd(0xb8);
		ser_wr_dat(0x2f);
		ser_wr_dat(0x2b);
		ser_wr_dat(0x2f);
	 
		ser_wr_cmd(0xbb);
		ser_wr_dat(0x24);
	 
		ser_wr_cmd(0xc0);
		ser_wr_dat(0x2C);
	 
		ser_wr_cmd(0xc3);
		ser_wr_dat(0x10);
	 
		ser_wr_cmd(0xc4);
		ser_wr_dat(0x20);
	 
		ser_wr_cmd(0xc6);
		ser_wr_dat(0x11);
	 
		ser_wr_cmd(0xd0);
		ser_wr_dat(0xa4);
		ser_wr_dat(0xa1);
	 
		ser_wr_cmd(0xe8);
		ser_wr_dat(0x03);
	 
		ser_wr_cmd(0xe9);
		ser_wr_dat(0x0d);
		ser_wr_dat(0x12);
		ser_wr_dat(0x00);
	 
		ser_wr_cmd(0xe0);
		ser_wr_dat(0xd0);
		ser_wr_dat(0x00);
		ser_wr_dat(0x00);
		ser_wr_dat(0x08);
		ser_wr_dat(0x11);
		ser_wr_dat(0x1a);
		ser_wr_dat(0x2b);
		ser_wr_dat(0x33);
		ser_wr_dat(0x42);
		ser_wr_dat(0x26);
		ser_wr_dat(0x12);
		ser_wr_dat(0x21);
		ser_wr_dat(0x2f);
		ser_wr_dat(0x11);
	 
		ser_wr_cmd(0xe1);
		ser_wr_dat(0xd0);
		ser_wr_dat(0x02);
		ser_wr_dat(0x09);
		ser_wr_dat(0x0d);
		ser_wr_dat(0x0d);
		ser_wr_dat(0x27);
		ser_wr_dat(0x2b);
		ser_wr_dat(0x33);
		ser_wr_dat(0x42);
		ser_wr_dat(0x17);
		ser_wr_dat(0x12);
		ser_wr_dat(0x11);
		ser_wr_dat(0x2f);
		ser_wr_dat(0x31);
	 
		ser_wr_cmd(0x21);
	 
		ser_wr_cmd(0xb0);
		ser_wr_dat(0x11); // rgb interface
		ser_wr_dat(0x00); 
		ser_wr_dat(0x00); 
	 
		ser_wr_cmd(0xb1);
		ser_wr_dat(0x40); // de mode
		ser_wr_dat(0x00); 
		ser_wr_dat(0x00); // bpb, hpb 
	 
		ser_wr_cmd(0x3a); 
		ser_wr_dat(0x55);
	 
		ser_wr_cmd(0x2a);
		ser_wr_dat(0x00);
		ser_wr_dat(0x00);
		ser_wr_dat(0x00);
		ser_wr_dat(0xef);
	 
		ser_wr_cmd(0x2b);
		ser_wr_dat(0x00);
		ser_wr_dat(0x00);
		ser_wr_dat(0x01);
		ser_wr_dat(0x3f);
	 
		ser_wr_cmd(0x11); 
		mdelay(120); 
		ser_wr_cmd(0x29);
		mdelay(120);
		ser_wr_cmd(0x2c);
		mdelay(120);
  #else
    ser_wr_cmd(0x36);
    ser_wr_dat(0x00);

    ser_wr_cmd(0xb2); // porch
    ser_wr_dat(0x08); // back porch
    ser_wr_dat(0x08); // front porch
    ser_wr_dat(0x00); // psen
    ser_wr_dat(0x88);
    ser_wr_dat(0x88);
   
    ser_wr_cmd(0xb7);
    ser_wr_dat(0x35);
    
    // Power settings
    ser_wr_cmd(0xbb);
    ser_wr_dat(0x15);
   
    ser_wr_cmd(0xc0);
    ser_wr_dat(0x6e);

    ser_wr_cmd(0xc2);
    ser_wr_dat(0x01);

    ser_wr_cmd(0xc3);
    ser_wr_dat(0x0b);
   
    ser_wr_cmd(0xc4);
    ser_wr_dat(0x20);
   
    ser_wr_cmd(0xc6);
    ser_wr_dat(0x0f);
   
    ser_wr_cmd(0xca);
    ser_wr_dat(0x0f);

    ser_wr_cmd(0xc8);
    ser_wr_dat(0x08);

    ser_wr_cmd(0x55);
    ser_wr_dat(0x90);

    ser_wr_cmd(0xd0);
    ser_wr_dat(0xa4);
    ser_wr_dat(0xa1);
    
    ser_wr_cmd(0xe0);
    ser_wr_dat(0xd0);
    ser_wr_dat(0x00);
    ser_wr_dat(0x00);
    ser_wr_dat(0x08);
    ser_wr_dat(0x11);
    ser_wr_dat(0x1a);
    ser_wr_dat(0x2b);
    ser_wr_dat(0x33);
    ser_wr_dat(0x42);
    ser_wr_dat(0x26);
    ser_wr_dat(0x12);
    ser_wr_dat(0x21);
    ser_wr_dat(0x2f);
    ser_wr_dat(0x11);
   
    ser_wr_cmd(0xe1);
    ser_wr_dat(0xd0);
    ser_wr_dat(0x02);
    ser_wr_dat(0x09);
    ser_wr_dat(0x0d);
    ser_wr_dat(0x0d);
    ser_wr_dat(0x27);
    ser_wr_dat(0x2b);
    ser_wr_dat(0x33);
    ser_wr_dat(0x42);
    ser_wr_dat(0x17);
    ser_wr_dat(0x12);
    ser_wr_dat(0x11);
    ser_wr_dat(0x2f);
    ser_wr_dat(0x31);

    ser_wr_cmd(0x21);
   
    ser_wr_cmd(0xb0);
    ser_wr_dat(0x11); // rgb interface
    ser_wr_dat(0x00);
   
    ser_wr_cmd(0xb1);
    ser_wr_dat(0x40); // rgb mode
    ser_wr_dat(0x08); // vbp 
    ser_wr_dat(0x08); // hbp

    ser_wr_cmd(0x3a); 
    ser_wr_dat(0x55); // 18bit=0x66, 16bit=0x55

    ser_wr_cmd(0x2b);
    ser_wr_dat(0x00);
    ser_wr_dat(0x00);
    ser_wr_dat(0x00);
    ser_wr_dat(0xef);
   
    ser_wr_cmd(0x2a);
    ser_wr_dat(0x00);
    ser_wr_dat(0x00);
    ser_wr_dat(0x01);
    ser_wr_dat(0x3f);

    ser_wr_cmd(0x11); 
    mdelay(120); 
    ser_wr_cmd(0x29);
    mdelay(120);
    ser_wr_cmd(0x2c); 
    mdelay(120);
  #endif
    ser_deinit();
    break;
  }

#if defined(MIYOO3)
  for(x=0;x<320;x++) gpio_wr_dat(0x00);
#endif
  writel(0xffffffff, iomm.gpio + PD_DATA);
  ret = readl(iomm.gpio + PD_CFG0);
  ret&= 0x0000000f;
  ret|= 0x22222220;
  writel(ret, iomm.gpio + PD_CFG0);
    ret = readl(iomm.gpio + PD_CFG1);
    ret&= 0x000000f0;
    ret|= 0x22222212;
    writel(ret, iomm.gpio + PD_CFG1);
  writel(0x00222222, iomm.gpio + PD_CFG2);
  return miyoo_ver;
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
  uint32_t ret=0, bp=0, total=0, ver=0;
	uint32_t h_front_porch = 8;
	uint32_t h_back_porch = 8;
	uint32_t h_sync_len = 1;
	uint32_t v_front_porch = 8;
	uint32_t v_back_porch = 8;
	uint32_t v_sync_len = 1;
      
  ser_init();
  ser_wr_cmd(0x04);
  if(ser_rd_dat() == 0x42c2a97f){
    ver = 4;
  }
  ser_deinit();

  if (version == 4 && ver != 4) {
    printk("Warning: LCD controller not detected as version 4, but being overridden by module_param as version 4 (detection 1 of 2)");
    ver = version;
  }
    suniv_setbits(iomm.lcdc + TCON1_CTRL_REG, (0 << 31));
    suniv_setbits(iomm.lcdc + TCON_CTRL_REG, (0 << 31));
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
	writel((((par->mode.yres) - 1) << 16) | (((par->mode.xres) - 1) << 0), iomm.debe + DEBE_DISP_SIZE_REG);
	writel((((par->mode.yres) - 1) << 16) | (((par->mode.xres) - 1) << 0), iomm.debe + DEBE_LAY0_SIZE_REG);
	writel((((par->mode.yres) - 1) << 16) | (((par->mode.xres) - 1) << 0), iomm.debe + DEBE_LAY1_SIZE_REG);
	writel((5 << 8), iomm.debe + DEBE_LAY0_ATT_CTRL_REG1);
  writel((5 << 8), iomm.debe + DEBE_LAY1_ATT_CTRL_REG1);
    ret = readl(iomm.debe + DEBE_LAY0_ATT_CTRL_REG0 ) & ~(3 << 1);
    writel(ret | (0 << 1), iomm.debe + DEBE_LAY0_ATT_CTRL_REG0);
	suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
	suniv_setbits(iomm.debe + DEBE_REGBUFF_CTRL_REG, (1 << 1));
  suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 1));

	ret = readl(iomm.lcdc + TCON_CTRL_REG);
	ret&= ~(1 << 0);
	writel(ret, iomm.lcdc + TCON_CTRL_REG);
	ret = (v_front_porch + v_back_porch + v_sync_len);
  if(ver == 4){
    writel((uint32_t)(par->lram_phys[0]) << 3, iomm.debe + DEBE_LAY0_FB_ADDR_REG);
    writel((uint32_t)(par->lram_phys[0]) >> 29, iomm.debe + DEBE_LAY0_FB_HI_ADDR_REG);
    writel((uint32_t)(par->lram_phys[0]) << 3, iomm.debe + DEBE_LAY1_FB_ADDR_REG);
    writel((uint32_t)(par->lram_phys[0]) >> 29, iomm.debe + DEBE_LAY1_FB_HI_ADDR_REG);
    
    //writel((uint32_t)(par->vram_phys) << 3, iomm.debe + DEBE_LAY0_FB_ADDR_REG);
    //writel((uint32_t)(par->vram_phys) >> 29, iomm.debe + DEBE_LAY0_FB_HI_ADDR_REG);
    //writel((uint32_t)(par->vram_phys + 320*240*2) << 3, iomm.debe + DEBE_LAY1_FB_ADDR_REG);
    //writel((uint32_t)(par->vram_phys + 320*240*2) >> 29, iomm.debe + DEBE_LAY1_FB_HI_ADDR_REG);
	  
    writel((1 << 31) | ((ret & 0x1f) << 4) | (0 << 24), iomm.lcdc + TCON0_CTRL_REG);
		writel(0x11111111, iomm.lcdc + TCON_FRM_SEED0_R_REG);
		writel(0x11111111, iomm.lcdc + TCON_FRM_SEED0_G_REG);
		writel(0x11111111, iomm.lcdc + TCON_FRM_SEED0_B_REG);
		writel(0x11111111, iomm.lcdc + TCON_FRM_SEED1_R_REG);
		writel(0x11111111, iomm.lcdc + TCON_FRM_SEED1_G_REG);
		writel(0x11111111, iomm.lcdc + TCON_FRM_SEED1_B_REG);
		writel(0x01010000, iomm.lcdc + TCON_FRM_TBL_REG0);
		writel(0x15151111, iomm.lcdc + TCON_FRM_TBL_REG1);
		writel(0x57575555, iomm.lcdc + TCON_FRM_TBL_REG2);
		writel(0x7f7f7777, iomm.lcdc + TCON_FRM_TBL_REG3);
		//writel((1 << 31) | (0 << 4), iomm.lcdc + TCON_FRM_CTRL_REG); // 18bit
		writel((1 << 31) | (5 << 4), iomm.lcdc + TCON_FRM_CTRL_REG); // 16bit
	  writel((0xf << 28) | (6 << 0), iomm.lcdc + TCON_CLK_CTRL_REG); // 6, 15
	  writel((1 << 28) | (1 << 25) | (1 << 24), iomm.lcdc + TCON0_IO_CTRL_REG0);

    p = par->lram_virt[0];
    for(y=0; y<240; y++){
      for(x=0; x<320; x++){
        if(y < 80){
          *p++ = 0xf800;
        }
        else if(y < 160){
          *p++ = 0x7e0;
        }
        else{
          *p++ = 0x1f;
        }
      }
    }
  
    //suniv_setbits(iomm.gpio + PE_CFG0, 1 << 16);
    //suniv_setbits(iomm.gpio + PE_CFG0, 1 << 17);
    //suniv_clrbits(iomm.gpio + PE_CFG0, 1 << 18);
    
    //suniv_setbits(iomm.gpio + PE_CFG0, 1 << 20);
    //suniv_setbits(iomm.gpio + PE_CFG0, 1 << 21);
    //suniv_clrbits(iomm.gpio + PE_CFG0, 1 << 22);
  }
  else{
    writel((uint32_t)(par->vram_phys) << 3, iomm.debe + DEBE_LAY0_FB_ADDR_REG);
    writel((uint32_t)(par->vram_phys) >> 29, iomm.debe + DEBE_LAY0_FB_HI_ADDR_REG);
    writel((uint32_t)(par->vram_phys + 320*240*2) << 3, iomm.debe + DEBE_LAY1_FB_ADDR_REG);
    writel((uint32_t)(par->vram_phys + 320*240*2) >> 29, iomm.debe + DEBE_LAY1_FB_HI_ADDR_REG);
	  
    writel((1 << 31) | ((ret & 0x1f) << 4) | (1 << 24), iomm.lcdc + TCON0_CTRL_REG);
	  writel((0xf << 28) | (6 << 0), iomm.lcdc + TCON_CLK_CTRL_REG); // 6, 15
	  writel((4 << 29) | (1 << 26), iomm.lcdc + TCON0_CPU_IF_REG);
	  writel((1 << 28), iomm.lcdc + TCON0_IO_CTRL_REG0);
  }

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
  printk("miyoo panel config as v%d\n", panel_init()); 
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

  //writel(0x90001c01, iomm.ccm + PLL_DDR_CTRL_REG);
  //while((readl(iomm.ccm + PLL_DDR_CTRL_REG) & (1 << 28)) == 0){}
  ddr_clock = readl(iomm.ccm + PLL_DDR_CTRL_REG);
#if defined(DEBUG)
  printk("DDR_clock set to 0x%x", (uint32_t)ddr_clock);
#endif
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
#if defined(MIYOO3)
    w = copy_to_user((void*)arg, &miyoo_ver, sizeof(uint32_t));
    break;
#elif defined(POCKETGO)
    w = copy_to_user((void*)arg, &miyoo_ver_temp, sizeof(uint32_t));
    break
#else
    w = copy_to_user((void*)arg, &miyoo_ver, sizeof(uint32_t));
    break;
#endif
  case MIYOO_FB0_SET_FLIP:
    flip = (bool)arg;
    ret = readl(iomm.gpio + PD_CFG0);
    ret&= 0x0000000f;
    ret|= 0x11111110;
    writel(0x11111101, iomm.gpio + PD_CFG1);
    writel(0x00111111, iomm.gpio + PD_CFG2);
    writel(0xffffffff, iomm.gpio + PD_DATA);
    mdelay(50);
    gpio_wr_cmd(0x28);
    mdelay(250);
    gpio_wr_cmd(0x36);
    if(flip){
    	gpio_wr_dat(0xB0); //screen direction //0x70 for 3.5, 0xB0 for pg
    } else {
    	gpio_wr_dat(0x70); //screen direction //0x70 for 3.5, 0xB0 for pg
    }
    mdelay(50);
    gpio_wr_cmd(0x29);
    mdelay(50);
    gpio_wr_cmd(0x2c);
    mdelay(100);
    writel(0xffffffff, iomm.gpio + PD_DATA);
    ret = readl(iomm.gpio + PD_CFG0);
    ret&= 0x0000000f;
    ret|= 0x22222220;
    writel(ret, iomm.gpio + PD_CFG0);
          ret = readl(iomm.gpio + PD_CFG1);
          ret&= 0x000000f0;
          ret|= 0x22222212;
          writel(ret, iomm.gpio + PD_CFG1);
    writel(0x00222222, iomm.gpio + PD_CFG2);
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
    suniv_ioremap();
    alloc_chrdev_region(&major, 0, 1, "miyoo_fb0");
    myclass = class_create(THIS_MODULE, "miyoo_fb0");
    device_create(myclass, NULL, major, NULL, "miyoo_fb0");
    cdev_init(&mycdev, &myfops);
    cdev_add(&mycdev, major, 1);
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
