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
#include <linux/sched.h>
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

#define PALETTE_SIZE 256
#define DRIVER_NAME  "q8-fb"
#define MIYOO_FB0_PUT_OSD     _IOWR(0x100, 0, unsigned long)
#define MIYOO_FB0_SET_MODE    _IOWR(0x101, 0, unsigned long)

static bool invert=false;
module_param(invert,bool,0660);

struct myfb_par {
    struct device *dev;
    struct platform_device *pdev;

    resource_size_t p_palette_base;
    unsigned short *v_palette_base;

    int yoffset;
    void *vram_virt;
    uint32_t vram_size;
    dma_addr_t vram_phys;

    int bpp;
    int lcdc_irq;
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
struct timer_list mytimer;
static struct suniv_iomm iomm={0};
static struct myfb_par *mypar=NULL;
static struct fb_var_screeninfo myfb_var={0};

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
    r&= 0xff000000;
    r|= 0x00222222;
    writel(r, iomm.gpio + PD_CFG2);

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
        suniv_setbits(iomm.lcdc + TCON0_CPU_IF_REG, (1 << 25));
    }
    else{
        suniv_clrbits(iomm.lcdc + TCON0_CPU_IF_REG, (1 << 25));
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
    suniv_setbits(iomm.lcdc + TCON_CTRL_REG, (1 << 31));
}

static irqreturn_t lcdc_irq_handler(int irq, void *arg)
{
    refresh_lcd(mypar);
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
    
    lcdc_wr_cmd(0x11);       // SleepIn
    mdelay(120);
    lcdc_wr_cmd(0x28);  // display off
    //------------- display control setting -----------------------//
    lcdc_wr_cmd(0xfe);
    lcdc_wr_cmd(0xef);
    lcdc_wr_cmd(0x36);
    //  lcdc_wr_dat(0x48);      // 原始方向：    Y=0 X=1 V=0 L=0     0x48
    lcdc_wr_dat(0x28);
    lcdc_wr_cmd(0x3a);
    lcdc_wr_dat(0x05);

    lcdc_wr_cmd(0x35);
    lcdc_wr_dat(0x00);
    lcdc_wr_cmd(0x44);
    lcdc_wr_dat(0x00);
    lcdc_wr_dat(0x60);

    //------end display control setting----//
    //------Power Control Registers Initial----//
    lcdc_wr_cmd(0xa4);
    lcdc_wr_dat(0x44);
    lcdc_wr_dat(0x44);
    lcdc_wr_cmd(0xa5);
    lcdc_wr_dat(0x42);
    lcdc_wr_dat(0x42);
    lcdc_wr_cmd(0xaa);
    lcdc_wr_dat(0x88);
    lcdc_wr_dat(0x88);
    lcdc_wr_cmd(0xe8);
    lcdc_wr_dat(0x11);
    lcdc_wr_dat(0x71);
    lcdc_wr_cmd(0xe3);
    lcdc_wr_dat(0x01);
    lcdc_wr_dat(0x10);
    lcdc_wr_cmd(0xff);
    lcdc_wr_dat(0x61);
    lcdc_wr_cmd(0xAC);
    lcdc_wr_dat(0x00);

    lcdc_wr_cmd(0xAe);
    lcdc_wr_dat(0x2b);//20161020

    lcdc_wr_cmd(0xAd);
    lcdc_wr_dat(0x33);
    lcdc_wr_cmd(0xAf);
    lcdc_wr_dat(0x55);
    lcdc_wr_cmd(0xa6);
    lcdc_wr_dat(0x2a);
    lcdc_wr_dat(0x2a);
    lcdc_wr_cmd(0xa7);
    lcdc_wr_dat(0x2b);
    lcdc_wr_dat(0x2b);
    lcdc_wr_cmd(0xa8);
    lcdc_wr_dat(0x18);
    lcdc_wr_dat(0x18);
    lcdc_wr_cmd(0xa9);
    lcdc_wr_dat(0x2a);
    lcdc_wr_dat(0x2a);
    //-----display window 240X320---------//
    lcdc_wr_cmd(0x2a);
    lcdc_wr_dat(0x00);
    lcdc_wr_dat(0x00);
    lcdc_wr_dat(0x01);
    lcdc_wr_dat(0x3f);
    lcdc_wr_cmd(0x2b);       // 0x002B = 239
    lcdc_wr_dat(0x00);
    lcdc_wr_dat(0x00);
    lcdc_wr_dat(0x00);
    lcdc_wr_dat(0xef);      // 0x013F = 319
    if (invert) {
        lcdc_wr_cmd(0x21); // Display Inversion On (for colors)
    } else {
        lcdc_wr_cmd(0x20); //  Display Inversion Off (for colors)
    }
    //    lcdc_wr_cmd(0x2c);
    //--------end display window --------------//
    //------------gamma setting------------------//
    lcdc_wr_cmd(0xf0);
    lcdc_wr_dat(0x02);
    lcdc_wr_dat(0x01);
    lcdc_wr_dat(0x00);
    lcdc_wr_dat(0x00);
    lcdc_wr_dat(0x02);
    lcdc_wr_dat(0x09);

    lcdc_wr_cmd(0xf1);
    lcdc_wr_dat(0x01);
    lcdc_wr_dat(0x02);
    lcdc_wr_dat(0x00);
    lcdc_wr_dat(0x11);
    lcdc_wr_dat(0x1c);
    lcdc_wr_dat(0x15);

    lcdc_wr_cmd(0xf2);
    lcdc_wr_dat(0x0a);
    lcdc_wr_dat(0x07);
    lcdc_wr_dat(0x29);
    lcdc_wr_dat(0x04);
    lcdc_wr_dat(0x04);
    lcdc_wr_dat(0x38);//v43n  39

    lcdc_wr_cmd(0xf3);
    lcdc_wr_dat(0x15);
    lcdc_wr_dat(0x0d);
    lcdc_wr_dat(0x55);
    lcdc_wr_dat(0x04);
    lcdc_wr_dat(0x03);
    lcdc_wr_dat(0x65);//v43p 66

    lcdc_wr_cmd(0xf4);
    lcdc_wr_dat(0x0f);//v50n
    lcdc_wr_dat(0x1d);//v57n
    lcdc_wr_dat(0x1e);//v59n
    lcdc_wr_dat(0x0a);//v61n 0b
    lcdc_wr_dat(0x0d);//v62n 0d
    lcdc_wr_dat(0x0f);

    lcdc_wr_cmd(0xf5);
    lcdc_wr_dat(0x05);//v50p
    lcdc_wr_dat(0x12);//v57p
    lcdc_wr_dat(0x11);//v59p
    lcdc_wr_dat(0x34);//v61p 35
    lcdc_wr_dat(0x34);//v62p 34
    lcdc_wr_dat(0x0f);
    //-------end gamma setting----//
    lcdc_wr_cmd(0x11);       // SleepOut
    mdelay(120);
    lcdc_wr_cmd(0x29);       // Display ON
    lcdc_wr_cmd(0x2c);       // Display ON

    mypar->yoffset = 0;
    memset(mypar->vram_virt, 0, 320*240*4);
}

static void suniv_lcdc_init(struct myfb_par *par)
{
	uint32_t ret=0, p1=0, p2=0;

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
	ret = (1 + 1 + 1);
	writel((uint32_t)(par->vram_phys + 320*240*2*0) << 3, iomm.debe + DEBE_LAY0_FB_ADDR_REG);
	writel((uint32_t)(par->vram_phys + 320*240*2*1) << 3, iomm.debe + DEBE_LAY1_FB_ADDR_REG);
	writel((uint32_t)(par->vram_phys + 320*240*2*2) << 3, iomm.debe + DEBE_LAY2_FB_ADDR_REG);
	writel((uint32_t)(par->vram_phys + 320*240*2*3) << 3, iomm.debe + DEBE_LAY3_FB_ADDR_REG);

	writel((uint32_t)(par->vram_phys + 320*240*2*0) >> 29, iomm.debe + DEBE_LAY0_FB_HI_ADDR_REG);
	writel((uint32_t)(par->vram_phys + 320*240*2*1) >> 29, iomm.debe + DEBE_LAY1_FB_HI_ADDR_REG);
	writel((uint32_t)(par->vram_phys + 320*240*2*2) >> 29, iomm.debe + DEBE_LAY2_FB_HI_ADDR_REG);
	writel((uint32_t)(par->vram_phys + 320*240*2*3) >> 29, iomm.debe + DEBE_LAY3_FB_HI_ADDR_REG);	

	writel((1 << 31) | ((ret & 0x1f) << 4) | (1 << 24), iomm.lcdc + TCON0_CTRL_REG);
	writel((0xf << 28) | (25 << 0), iomm.lcdc + TCON_CLK_CTRL_REG); //6, 15, 25
	writel((4 << 29) | (1 << 26), iomm.lcdc + TCON0_CPU_IF_REG);
	writel((1 << 28), iomm.lcdc + TCON0_IO_CTRL_REG0);

	p1 = par->mode.yres - 1;
	p2 = par->mode.xres - 1;
	writel((p2 << 16) | (p1 << 0), iomm.lcdc + TCON0_BASIC_TIMING_REG0);

	p1 = 1 + 1;
	p2 = 1 + 1 + par->mode.xres + 2;
	writel((p2 << 16) | (p1 << 0), iomm.lcdc + TCON0_BASIC_TIMING_REG1);

	p1 = 1 + 1;
	p2 = (1 + 1 + par->mode.yres + 1 + 2) << 1;
	writel((p2 << 16) | (p1 << 0), iomm.lcdc + TCON0_BASIC_TIMING_REG2);

	p1 = 1 + 1;
	p2 = 1 + 1;
	writel((p2 << 16) | (p1 << 0), iomm.lcdc + TCON0_BASIC_TIMING_REG3);

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

    par->lcdc_irq = platform_get_irq(par->pdev, 0);
    if(par->lcdc_irq < 0){
        printk("%s, failed to get irq number for lcdc irq\n", __func__);
    }
    else{
        ret = request_irq(par->lcdc_irq, lcdc_irq_handler, IRQF_SHARED, "lcdc_irq", par);
        if(ret){
            printk("%s, failed to register lcdc interrupt(%d)\n", __func__, par->lcdc_irq);
        }
    }
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
    suniv_setbits(iomm.ccm + TCON_CLK_REG, (1 << 31) | (1 << 25));
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
    int ret=0;
    struct fb_info *info=NULL;
    struct myfb_par *par=NULL;
    struct fb_videomode *mode=NULL;

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
        return -ENOMEM;
    }

    par = info->par;
    par->pdev = device;
    par->dev = &device->dev;
    par->bpp = 16;
    fb_videomode_to_var(&myfb_var, mode);

    par->vram_size = 320 * 240 * 2 * 2;
    par->vram_virt = dma_alloc_coherent(par->dev, par->vram_size, (resource_size_t*)&par->vram_phys, GFP_KERNEL | GFP_DMA);
    if(!par->vram_virt){
        return -EINVAL;
    }
    info->screen_base = (char __iomem*)par->vram_virt;
    myfb_fix.smem_start = par->vram_phys;
    myfb_fix.smem_len = par->vram_size;
    myfb_fix.line_length = 320 * 2;

    par->v_palette_base = dma_alloc_coherent(par->dev, PALETTE_SIZE, (resource_size_t*)&par->p_palette_base, GFP_KERNEL | GFP_DMA);
    if(!par->v_palette_base){
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
        return -EINVAL;
    }

    mypar = par;
    for(ret=0; ret<of_clk_get_parent_count(device->dev.of_node); ret++){
        clk_prepare_enable(of_clk_get(device->dev.of_node, ret));
    }
    fb_prepare_logo(info, 0);
    fb_show_logo(info, 0);

    suniv_cpu_init(mypar);
    suniv_lcdc_init(mypar);
    suniv_enable_irq(mypar);

    return 0;
}

static int myfb_remove(struct platform_device *dev)
{
    struct fb_info *info = dev_get_drvdata(&dev->dev);
    struct myfb_par *par = info->par;

    if(info){
        free_irq(par->lcdc_irq, par);
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
    int32_t w, bpp;

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

MODULE_DESCRIPTION("Framebuffer driver for GC9306");
MODULE_AUTHOR("Steward Fu <steward.fu@gmail.com>");
MODULE_LICENSE("GPL");

