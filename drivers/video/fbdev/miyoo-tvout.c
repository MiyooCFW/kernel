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
#include <asm/arch-suniv/tve.h>

#define PALETTE_SIZE 256
#define DRIVER_NAME  "tvout-fb"
#define MIYOO_FB0_PUT_OSD     _IOWR(0x100, 0, unsigned long)
#define MIYOO_FB0_SET_MODE    _IOWR(0x101, 0, unsigned long)
static int tvmode=0;
module_param(tvmode,int,0660);


struct myfb_app{
    uint32_t yoffset;
    uint32_t vsync_count;
};

struct myfb_par {
    struct device *dev;
    struct platform_device *pdev;

    resource_size_t p_palette_base;
    unsigned short *v_palette_base;

    dma_addr_t vram_phys;
    uint32_t vram_size;
    void *vram_virt;
    int yoffset;
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
    uint8_t *lcdc;
    uint8_t *debe;
    uint8_t *intc;
    uint8_t *timer;
    uint8_t *tve;
    uint8_t *defe;
};
static int major = -1;
static struct cdev mycdev;
static struct class *myclass = NULL;
struct timer_list mytimer;
static struct suniv_iomm iomm={0};
static struct myfb_par *mypar=NULL;
static struct fb_var_screeninfo myfb_var={0};

uint16_t i;
uint32_t val;
tve_mode_e mode;
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

static const uint32_t csc_tab[192] =
        {
                //Y/G   Y/G     Y/G     Y/G     U/R     U/R     U/R     U/R     V/B     V/B     V/B     V/B
                //bt601
                0x04a8, 0x1e70, 0x1cbf, 0x0878, 0x04a8, 0x0000, 0x0662, 0x3211, 0x04a8, 0x0812, 0x0000, 0x2eb1, //yuv2rgb
                0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //yuv2yuv
                0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //rgb2rgb
                0x0204, 0x0107, 0x0064, 0x0100, 0x1ed6, 0x1f68, 0x01c2, 0x0800, 0x1e87, 0x01c2, 0x1fb7, 0x0800, //rgb2yuv

                //bt709
                0x04a8, 0x1f26, 0x1ddd, 0x04d0, 0x04a8, 0x0000, 0x072c, 0x307e, 0x04a8, 0x0876, 0x0000, 0x2dea, //yuv2rgb
                0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //yuv2yuv
                0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //rgb2rgb
                0x0275, 0x00bb, 0x003f, 0x0100, 0x1ea6, 0x1f99, 0x01c2, 0x0800, 0x1e67, 0x01c2, 0x1fd7, 0x0800, //rgb2yuv

                //DISP_ YCC
                0x0400, 0x1e9e, 0x1d24, 0x087b, 0x0400, 0x0000, 0x059c, 0x34c8, 0x0400, 0x0716, 0x0000, 0x31d5, //yuv2rgb
                0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //yuv2yuv
                0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //rgb2rgb
                0x0259, 0x0132, 0x0075, 0x0000, 0x1ead, 0x1f53, 0x0200, 0x0800, 0x1e54, 0x0200, 0x1fac, 0x0800, //rgb2yuv

                //xvYCC
                0x04a8, 0x1f26, 0x1ddd, 0x04d0, 0x04a8, 0x0000, 0x072c, 0x307e, 0x04a8, 0x0876, 0x0000, 0x2dea, //yuv2rgb
                0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //yuv2yuv
                0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //rgb2rgb
                0x0275, 0x00bb, 0x003f, 0x0100, 0x1ea6, 0x1f99, 0x01c2, 0x0800, 0x1e67, 0x01c2, 0x1fd7, 0x0800, //rgb2yuv
        };

typedef struct
{
    uint16_t width;
    uint16_t fbwidth;
    uint16_t height;
    uint8_t bits_per_pixel;
} de_layer_params_t;

typedef struct
{
    uint32_t width;
    uint32_t height;
    de_layer_params_t layer[4];
    de_mode_e mode;
} de_params_t;

static de_params_t de;


static uint32_t pll_video_get_freq(void) // +
{
    uint32_t reg = readl(iomm.ccm + PLL_VIDEO_CTRL_REG);

    if ((reg & (1 << 24)) == 0)
    {                           // Fractional mode
        if (reg & (1 << 25))
            return 297000000;
        else
            return 270000000;
    }
    else
    {                           // Integer mode
        uint32_t mul = (reg >> 8) & 0x7F;
        uint32_t div = (reg >> 0) & 0xF;

        return (24000000 * (mul + 1) / (div + 1));
    }
}

// Video encoder clock configuration
void clk_tve_config(uint8_t div) // todo: source select
{
    if ((div == 0) || (div > 16))
        return;
    writel((0x80008100) | (div-1), iomm.ccm + TVE_CLK_REG );
}

void tve_init(tve_mode_e mode)
{

    // Determine tve clock division value. PLL_VIDEO should be configured and enabled!
    uint32_t tve_clk_div = pll_video_get_freq() / 27000000LU;
    clk_tve_config(tve_clk_div);
    suniv_setbits(iomm.ccm + BUS_CLK_GATING_REG1, (1 << 10));
    suniv_setbits(iomm.ccm + BUS_SOFT_RST_REG1, (1 << 10));

    writel(0x433810A1, iomm.tve +TVE_DAC1);
    if (mode == TVE_MODE_NTSC) // NTSC
    {
        writel(0x07030000, iomm.tve +TVE_CFG1 );
        writel(0x00000120, iomm.tve +TVE_NOTCH_DELAY );
        writel(0x21F07C1F, iomm.tve +TVE_CHROMA_FREQ );
        writel(0x00760020, iomm.tve +TVE_FB_PORCH );
        writel(0x00000016, iomm.tve +TVE_HD_VS );
        writel(0x0016020D, iomm.tve +TVE_LINE_NUM );
        writel(0x00F0011A, iomm.tve +TVE_LEVEL );
        writel(0x00000001, iomm.tve +TVE_CB_RESET );
        writel(0x00000000, iomm.tve +TVE_VS_NUM );
        writel(0x00000002, iomm.tve +TVE_FILTER );
        writel(0x0000004F, iomm.tve +TVE_CBCR_LEVEL );
        writel(0x00000000, iomm.tve +TVE_TINT_PHASE );
        writel(0x0016447E, iomm.tve +TVE_B_WIDTH );
        writel(0x0000A0A0, iomm.tve +TVE_CBCR_GAIN );
        writel(0x001000F0, iomm.tve +TVE_SYNC_LEVEL );
        writel(0x01E80320, iomm.tve +TVE_WHITE_LEVEL );
        writel(0x000005A0, iomm.tve +TVE_ACT_LINE );
        writel(0x00000000, iomm.tve +TVE_CHROMA_BW );
        writel(0x00000101, iomm.tve +TVE_CFG2 );
        writel(0x000E000C, iomm.tve +TVE_RESYNC );
        writel(0x00000000, iomm.tve +TVE_SLAVE );
        writel(0x00000000, iomm.tve +TVE_CFG3 );
        writel(0x00000000, iomm.tve +TVE_CFG4 );
    }
    else // PAL
    {
        writel(0x07030001, iomm.tve +TVE_CFG1 );
        writel(0x00000120, iomm.tve +TVE_NOTCH_DELAY );
        writel(0x2A098ACB, iomm.tve +TVE_CHROMA_FREQ );
        writel(0x008A0018, iomm.tve +TVE_FB_PORCH );
        writel(0x00000016, iomm.tve +TVE_HD_VS );
        writel(0x00160271, iomm.tve +TVE_LINE_NUM );
        writel(0x00FC00FC, iomm.tve +TVE_LEVEL );
        writel(0x00000000, iomm.tve +TVE_CB_RESET );
        writel(0x00000001, iomm.tve +TVE_VS_NUM );
        writel(0x00000005, iomm.tve +TVE_FILTER );
        writel(0x00002828, iomm.tve +TVE_CBCR_LEVEL );
        writel(0x00000000, iomm.tve +TVE_TINT_PHASE );
        writel(0x0016447E, iomm.tve +TVE_B_WIDTH );
        writel(0x0000E0E0, iomm.tve +TVE_CBCR_GAIN );
        writel(0x001000F0, iomm.tve +TVE_SYNC_LEVEL );
        writel(0x01E80320, iomm.tve +TVE_WHITE_LEVEL );
        writel(0x000005A0, iomm.tve +TVE_ACT_LINE );
        writel(0x00000000, iomm.tve +TVE_CHROMA_BW );
        writel(0x00000101, iomm.tve +TVE_CFG2 );
        writel(0x800D000C, iomm.tve +TVE_RESYNC );
        writel(0x00000000, iomm.tve +TVE_SLAVE );
        writel(0x00000000, iomm.tve +TVE_CFG3 );
        writel(0x00000000, iomm.tve +TVE_CFG4 );
    }
    writel(1, iomm.tve +TVE_ENABLE );
}

void tve_enable(void)
{
    suniv_setbits(iomm.tve +TVE_DAC1, 0x1 << 0);
    suniv_setbits(iomm.tve +TVE_ENABLE, 0x1 << 0);
}

static void tcon1_init(tve_mode_e mode) // TCON1 -> TVE
{
    uint32_t ret;
    writel(0, iomm.lcdc + TCON_CTRL_REG);
    writel(0, iomm.lcdc + TCON_INT_REG0);
    ret = readl(iomm.lcdc + TCON_CLK_CTRL_REG);
    ret&= ~(0xf << 28);
    writel(ret, iomm.lcdc + TCON_CLK_CTRL_REG);
    writel(0xffffffff, iomm.lcdc + TCON0_IO_CTRL_REG1);
    writel(0xffffffff, iomm.lcdc + TCON1_IO_CTRL_REG1);
    ret = readl(iomm.lcdc + TCON_CTRL_REG);
    ret&= ~(1 << 0);
    writel(ret, iomm.lcdc + TCON_CTRL_REG);
    if (mode == TVE_MODE_NTSC)
    {
        writel(0x00100130,iomm.lcdc + TCON1_CTRL_REG);
        writel(((720-1) << 16) | (480/2-1),iomm.lcdc + TCON1_BASIC_REG0);
        writel(((720-1) << 16) | (480/2-1),iomm.lcdc + TCON1_BASIC_REG1);
        writel(((720-1) << 16) | (480/2-1),iomm.lcdc + TCON1_BASIC_REG2);
        writel(((858-1) << 16) | (117),iomm.lcdc + TCON1_BASIC_REG3);
        writel((525 << 16) | (18),iomm.lcdc + TCON1_BASIC_REG4);
    }
    else if (mode == TVE_MODE_PAL)
    {
        writel(0x00100150,iomm.lcdc + TCON1_CTRL_REG);
        writel(((720-1) << 16) | (575/2-1),iomm.lcdc + TCON1_BASIC_REG0);
        writel(((720-1) << 16) | (575/2-1),iomm.lcdc + TCON1_BASIC_REG1);
        writel(((720-1) << 16) | (575/2-1),iomm.lcdc + TCON1_BASIC_REG2);
        writel(((864-1) << 16) | (138),iomm.lcdc + TCON1_BASIC_REG3);
        writel((625 << 16) | (22),iomm.lcdc + TCON1_BASIC_REG4);
    }

    writel(0x00010001,iomm.lcdc + TCON1_BASIC_REG5);
    writel(0x00000000,iomm.lcdc + TCON1_IO_CTRL_REG0);
    writel(0x0FFFFFFF,iomm.lcdc + TCON1_IO_CTRL_REG1);
}

static void debe_update_linewidth(uint8_t layer)
{
    if (layer > 3)
        return;

    val = de.layer[layer].width * de.layer[layer].bits_per_pixel;
    writel(val, iomm.debe + DEBE_LAY0_LINEWIDTH_REG + layer*4);
}

void debe_layer_set_size(uint8_t layer, uint16_t w, uint16_t h)
{
    if (layer > 3)
        return;
    de.layer[layer].width = w;
    de.layer[layer].height = h;

    writel(((h - 1) << 16) | (w - 1), iomm.debe + DEBE_LAY0_SIZE_REG + layer*4);

    debe_update_linewidth(layer);
}

void debe_layer_init(uint8_t layer)
{
    if (layer > 3)
        return;
    de.layer[layer].width = de.width;
    de.layer[layer].height = de.height;

    writel((20 << 0) | (20 << 16), iomm.debe + DEBE_LAY0_CODNT_REG + layer*4);
    writel(((de.height - 1) << 16) | (de.width - 1), iomm.debe + DEBE_LAY0_SIZE_REG + layer*4 );

    debe_update_linewidth(layer);
}

void debe_layer_set_mode(uint8_t layer, debe_color_mode_e mode)
{
    if (layer > 3)
        return;
    if (mode == DEBE_MODE_DEFE_VIDEO)
    {
        val = readl(iomm.debe + DEBE_LAY0_ATT_CTRL_REG0 + layer*4) & ~(3 << 1);
        writel(val | (1 << 1), iomm.debe + DEBE_LAY0_ATT_CTRL_REG0 + layer*4);
    }
    else if (mode == DEBE_MODE_YUV)
    {
        val = readl(iomm.debe + DEBE_LAY0_ATT_CTRL_REG0 + layer*4) & ~(3 << 1);
        writel(val | (1 << 2),iomm.debe + DEBE_LAY0_ATT_CTRL_REG0 + layer*4);
    }
    else
    {
        de.layer[layer].bits_per_pixel = (mode >> 8) & 0x00FF;

        if (mode & DEBE_PALETTE_EN)
            suniv_setbits(iomm.debe + DEBE_LAY0_ATT_CTRL_REG0 + layer*4, (1 << 22));
        else
            suniv_clrbits(iomm.debe + DEBE_LAY0_ATT_CTRL_REG0 + layer*4, (1 << 22));

        val = readl(iomm.debe + DEBE_LAY0_ATT_CTRL_REG1 + layer*4) & ~(0x0F << 8);
        writel(val | ((mode & 0x0F) << 8),iomm.debe + DEBE_LAY0_ATT_CTRL_REG1 + layer*4);

        debe_update_linewidth(layer);
    }
}

static void debe_init(struct myfb_par *par)
{
    writel((1 << 1), iomm.debe + DEBE_MODE_CTRL_REG);
    for (i = 0; i < 1; i++)
    {
        writel(0, iomm.debe + DEBE_LAY0_ATT_CTRL_REG1 + i*4);
        de.layer[i].bits_per_pixel = 32;
        debe_layer_init(i);
        debe_layer_set_mode(i, DEBE_MODE_DEFE_VIDEO);
    }
    for (i = 0; i < 4; i++) {
        writel(csc_tab[12 * 3 + i] << 16, iomm.debe + DEBE_COEF00_REG + i * 4 + 0 * 4);
        writel(csc_tab[12 * 3 + i + 4] << 16, iomm.debe + DEBE_COEF00_REG + i * 4 + 4 * 4);
        writel(csc_tab[12 * 3 + i + 8] << 16, iomm.debe + DEBE_COEF00_REG + i * 4 + 8 * 4);
    }
    suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 5)); // CSC enable
}

// TCON clock configuration
void clk_tcon_config(void)
{
    uint32_t val = readl(iomm.ccm + TCON_CLK_REG) & ~(0x7 << 24);
    writel( val | (0 << 24), iomm.ccm + TCON_CLK_REG);
}

void defe_init_spl_422(uint16_t in_w, uint16_t in_h, struct myfb_par *par)
{
    suniv_setbits(iomm.defe+DEFE_EN, 0x01); // Enable DEFE
    suniv_setbits(iomm.defe+DEFE_EN, (1 << 31)); // Enable CPU access
    writel((0 << 0) | (1 << 1), iomm.defe+DEFE_BYPASS); // CSC/scaler bypass disabled

    writel((uint32_t)((par->vram_phys)), iomm.defe+DEFE_ADDR0);
    writel(in_w*4, iomm.defe+DEFE_STRIDE0);
    writel((in_w-1) | ((in_h-1) << 16), iomm.defe+DEFE_IN_SIZE);
    if (tvmode == 0) {
        writel((665-1) | ((450-1) << 16), iomm.defe+DEFE_OUT_SIZE);
        writel((31500 << 0), iomm.defe+DEFE_H_FACT); // H scale
        writel((70600 << 0), iomm.defe+DEFE_V_FACT); // V scale
    }
    else {
        writel((665-1) | ((546-1) << 16), iomm.defe+DEFE_OUT_SIZE);
        writel((31500 << 0), iomm.defe+DEFE_H_FACT); // H scale
        writel((59000 << 0), iomm.defe+DEFE_V_FACT); // V scale
    }


    writel( (1 << 8 ) | (5 << 4) | (1 << 0), iomm.defe+DEFE_IN_FMT); // ARGB888
    writel( (1 << 4), iomm.defe+DEFE_OUT_FMT); // Output interlace enable
    suniv_setbits(iomm.defe+DEFE_FRM_CTRL, (1 << 23)); // Enable CPU access to filter RAM (if enabled, filter is bypassed?)
    suniv_clrbits(iomm.defe+DEFE_EN, (1 << 31)); // Disable CPU access (?)
    suniv_setbits(iomm.defe+DEFE_FRM_CTRL, (1 << 0)); // Registers ready
    suniv_setbits(iomm.defe+DEFE_FRM_CTRL, (1 << 16)); // Start frame processing
}

static void suniv_tve_init(struct myfb_par *par)
{
    de.mode = DE_TV;

    if (tvmode == 0) {
        mode = TVE_MODE_NTSC;
        de.width = 665;
        de.height = 450;
    }
    else {
        mode = TVE_MODE_PAL;
        de.width = 665;
        de.height = 546;
    }
    defe_init_spl_422(320, 240, par);
    debe_init(par);
    tcon1_init(mode);
    suniv_setbits(iomm.lcdc + TCON1_CTRL_REG, (1 << 31));
    suniv_setbits(iomm.tve + TVE_DAC1, 0x1 << 0);
    suniv_setbits(iomm.tve + TVE_ENABLE, 0x1 << 0);
    suniv_setbits(iomm.lcdc + TCON_CTRL_REG, (1 << 31));
    suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 0));
    suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
    tve_init(mode);
}

static void pll_video_init(uint8_t mul, uint8_t div) // out = (24MHz*N) / M
{
    if ((mul == 0) || (div == 0))
        return;
    if ((mul > 128) || (div > 16))
        return;

    // mul = n
    // div = m

    uint32_t val = readl(iomm.ccm + PLL_VIDEO_CTRL_REG);
    val &= (1 << 31) | (1 << 28);
    val |= ((mul - 1) << 8) | (div - 1) | (1 << 24);
    writel(val, iomm.ccm + PLL_VIDEO_CTRL_REG);
}

// CPU clock configuration
void clk_cpu_config(clk_source_cpu_e source)
{
    uint32_t reg = readl(iomm.ccm +CCU_CPU_CFG) & ~(0x3 << 16);
    writel( reg | (source << 16), iomm.ccm+CCU_CPU_CFG);
}

static void pll_periph_init(uint8_t mul, uint8_t div) // out = (24MHz*N) / M
{
    if ((mul == 0) || (div == 0))
        return;
    if ((mul > 32) || (div > 4))
        return;

    // mul = n
    // div = m

    uint32_t val = readl(iomm.ccm + CCU_PLL_PERIPH_CTRL);
    val &= (1 << 31) | (1 << 28);
    val |= ((mul - 1) << 8) | ((div - 1) << 4) | (1 << 18); // do we need 24m output?
    writel(val, iomm.ccm + CCU_PLL_PERIPH_CTRL);
}

// Enable PLL
inline void clk_pll_enable(pll_ch_e pll)
{
    writel((readl(iomm.ccm+pll)|(1 << 31)), iomm.ccm+pll);
}

// Get PLL lock state
uint8_t clk_pll_is_locked(pll_ch_e pll)
{
    uint32_t val = readl(iomm.ccm + pll);
    return ((val >> 28) & 0x1);
}

void clk_hclk_config(uint8_t div) // HCLK = CPUCLK / div
{
    if ((div == 0) || (div > 4))
        return;

    uint32_t val = readl(iomm.ccm+CCU_AHB_APB_CFG) & ~(0x3 << 16);
    writel(val | ((div-1) << 16), iomm.ccm+CCU_AHB_APB_CFG);
}

void clk_apb_config(clk_div_apb_e div) // APB = AHB / div
{
    uint32_t val = readl(iomm.ccm+CCU_AHB_APB_CFG) & ~(0x3 << 8);
    writel( val | (div << 8), iomm.ccm+CCU_AHB_APB_CFG);
}

void clk_ahb_config(clk_source_ahb_e src, uint8_t prediv, uint8_t div) // AHB = (src or src/prediv)/div
{
    if ((prediv == 0) || (prediv > 4))
        return;
    if ((div == 0) || ((div > 4) && (div != 8)) || (div == 3))
        return;
    if (div == 4)
        div = 3;
    if (div == 8)
        div = 4;

    uint32_t val = readl(iomm.ccm+CCU_AHB_APB_CFG) & ~((0x3 << 12) | (0xF << 4));
    writel(val | (src << 12) | ((prediv-1) << 6) | ((div-1) << 4), iomm.ccm+CCU_AHB_APB_CFG);
}

static void pll_cpu_init(uint8_t mul, uint8_t div) // out = (24MHz*N*K) / (M*P)
{
    if ((mul == 0) || (div == 0))
        return;
    if ((mul > 128) || (div > 16))
        return;

    uint8_t n, k, m, p;
    // mul = n*k
    // n = 1..32
    // k = 1..4
    for (k = 1; k <= 4; k++)
    {
        n = mul / k;
        if ((n < 32) && (n * k == mul))
            break;
    }
    if (n * k != mul)
        return;
    // div = m*p
    // m = 1..4
    // k = 1,2,4
    for (m = 1; m <= 4; m++)
    {
        p = div / m;
        if (((p == 1) || (p == 2) || (p == 4)) && (m * p == div))
            break;
    }
    if (m * p != div)
        return;

    p--;
    if (p == 3)
        p = 2;

    uint32_t val = readl(iomm.ccm + CCU_PLL_CPU_CTRL);
    val &= (1 << 31) | (1 << 28);
    val |= ((n - 1) << 8) | ((k - 1) << 4) | (m - 1) | (p << 16);
    writel(val, iomm.ccm + CCU_PLL_CPU_CTRL);
}

static void suniv_cpu_init(struct myfb_par *par)
{
    uint32_t ret, i;

    // Set CPU clock to 24MHz
    clk_cpu_config(CLK_CPU_SRC_OSC24M);
    mdelay(10); // Wait for some clock cycles

    pll_periph_init(25, 1); // PLL_PERIPH = 24M*25/1 = 600M
    clk_pll_enable(PLL_PERIPH);
    while (!clk_pll_is_locked(PLL_PERIPH)); // Wait for PLL to lock

    // Configure bus clocks
    clk_hclk_config(1); // HCLK = CLK_CPU
    clk_ahb_config(CLK_AHB_SRC_PLL_PERIPH_PREDIV, 3, 1); // AHB = PLL_PERIPH/3/1 = 200M
    clk_apb_config(CLK_APB_DIV_2); // APB = AHB/2 = 100M
    mdelay(10);

    // Configure video clocks
    pll_video_init(99,8); // 24*99/8 = 297MHz
    clk_pll_enable(PLL_VIDEO);

    // Configure CPU PLL
    pll_cpu_init(30, 1); // PLL_CPU = 24M*30/1 = 720M
    clk_pll_enable(PLL_CPU);
    while (!clk_pll_is_locked(PLL_CPU)); // Wait for PLL to lock

    // Select PLL as CPU clock source
    clk_cpu_config(CLK_CPU_SRC_PLL_CPU);
    mdelay(10);
    while((readl(iomm.ccm + PLL_VIDEO_CTRL_REG) & (1 << 28)) == 0){}
    while((readl(iomm.ccm + PLL_PERIPH_CTRL_REG) & (1 << 28)) == 0){}

    ret = readl(iomm.ccm + DRAM_GATING_REG);
    ret|= (1 << 26) | (1 << 24);
    writel(ret, iomm.ccm + DRAM_GATING_REG);

    suniv_setbits(iomm.ccm + FE_CLK_REG, (1 << 31));
    suniv_setbits(iomm.ccm + BE_CLK_REG, (1 << 31));
    suniv_setbits(iomm.ccm + TCON_CLK_REG, (1 << 31) | (0 << 25));
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

    if((var->xres != 320) || (var->yres != 240) || (var->bits_per_pixel != 32)){
        return -EINVAL;
    }

    var->transp.offset = 0;
    var->transp.length = 0;
    var->red.offset = 16;
    var->red.length = 8;
    var->green.offset = 8;
    var->green.length = 8;
    var->blue.offset = 0;
    var->blue.length = 8;
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

static int myfb_set_par(struct fb_info *info)
{
    struct myfb_par *par = info->par;

    fb_var_to_videomode(&par->mode, &info->var);
    par->yoffset = info->var.yoffset;
    par->bpp = info->var.bits_per_pixel;
    info->fix.visual = FB_VISUAL_TRUECOLOR;
    info->fix.line_length = (par->mode.xres * par->bpp) / 8;
    writel((9 << 8), iomm.debe + DEBE_LAY0_ATT_CTRL_REG1);
    writel((9 << 8), iomm.debe + DEBE_LAY1_ATT_CTRL_REG1);
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
    par->bpp = 32;
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
    myfb_fix.line_length = 320 * 4;

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
    for(ret=0; ret<of_clk_get_parent_count(device->dev.of_node); ret++){
        clk_prepare_enable(of_clk_get(device->dev.of_node, ret));
    }
    suniv_cpu_init(par);
    suniv_tve_init(par);

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
    iomm.lcdc = (uint8_t*)ioremap(SUNIV_LCDC_BASE, 4096);
    iomm.debe = (uint8_t*)ioremap(SUNIV_DEBE_BASE, 4096);
    iomm.defe = (uint8_t*)ioremap(SUNIV_DEFE_BASE, 4096);
    iomm.intc = (uint8_t*)ioremap(SUNIV_INTC_BASE, 4096);
    iomm.tve = (uint8_t*)ioremap(SUNIV_TVE_BASE, 1024);
}

static void suniv_iounmap(void)
{
    iounmap(iomm.dma);
    iounmap(iomm.ccm);
    iounmap(iomm.lcdc);
    iounmap(iomm.debe);
    iounmap(iomm.defe);
    iounmap(iomm.tve);
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
    alloc_chrdev_region(&major, 0, 1, "miyoo_tvout_fb0");
    myclass = class_create(THIS_MODULE, "miyoo_tvout_fb0");
    device_create(myclass, NULL, major, NULL, "miyoo_tvout_fb0");
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

MODULE_DESCRIPTION("Framebuffer driver for TVE");
MODULE_AUTHOR("Tiopex <tiopxyz@gmail.com>");
MODULE_LICENSE("GPL");
