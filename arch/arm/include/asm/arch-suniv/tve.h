/*
 * Sunxi TV encoder register and constant defines
 *
 * (C) Copyright 2014 Hans de Goede <hdegoede@redhat.com>
 * (C) Copyright 2017 Jernej Skrabec <jernej.skrabec@siol.net>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _TVE_H
#define _TVE_H

enum tve_mode {
	tve_mode_vga,
	tve_mode_composite_pal,
	tve_mode_composite_ntsc,
	tve_mode_composite_pal_m,
	tve_mode_composite_pal_nc,
};

/*
 * This is based on the A10s User Manual, and the A10s only supports
 * composite video and not vga like the A10 / A20 does, still other
 * than the removed vga out capability the tvencoder seems to be the same.
 * "unknown#" registers are registers which are used in the A10 kernel code,
 * but not documented in the A10s User Manual.
 */
struct sunxi_tve_reg {
	u32 gctrl;			/* 0x000 */
	u32 cfg0;			/* 0x004 */
	u32 dac_cfg0;			/* 0x008 */
	u32 filter;			/* 0x00c */
	u32 chroma_freq;		/* 0x010 */
	u32 porch_num;			/* 0x014 */
	u32 unknown0;			/* 0x018 */
	u32 line_num;			/* 0x01c */
	u32 blank_black_level;		/* 0x020 */
	u32 unknown1;			/* 0x024, seems to be 1 byte per dac */
	u8 res0[0x08];			/* 0x028 */
	u32 auto_detect_en;		/* 0x030 */
	u32 auto_detect_int_status;	/* 0x034 */
	u32 auto_detect_status;		/* 0x038 */
	u32 auto_detect_debounce;	/* 0x03c */
	u32 csc_reg0;			/* 0x040 */
	u32 csc_reg1;			/* 0x044 */
	u32 csc_reg2;			/* 0x048 */
	u32 csc_reg3;			/* 0x04c */
	u8 res1[0xb0];			/* 0x050 */
	u32 color_burst;		/* 0x100 */
	u32 vsync_num;			/* 0x104 */
	u32 notch_freq;			/* 0x108 */
	u32 cbr_level;			/* 0x10c */
	u32 burst_phase;		/* 0x110 */
	u32 burst_width;		/* 0x114 */
	u32 unknown2;			/* 0x118 */
	u32 sync_vbi_level;		/* 0x11c */
	u32 white_level;		/* 0x120 */
	u32 active_num;			/* 0x124 */
	u32 chroma_bw_gain;		/* 0x128 */
	u32 notch_width;		/* 0x12c */
	u32 resync_num;			/* 0x130 */
	u32 slave_para;			/* 0x134 */
	u32 cfg1;			/* 0x138 */
	u32 cfg2;			/* 0x13c */
};

/*
 * TVE register constants.
 */
#define SUNXI_TVE_GCTRL_ENABLE			(1 << 0)
/*
 * Select input 0 to disable dac, 1 - 4 to feed dac from tve0, 5 - 8 to feed
 * dac from tve1. When using tve1 the mux value must be written to both tve0's
 * and tve1's gctrl reg.
 */
#define SUNXI_TVE_GCTRL_DAC_INPUT_MASK(dac)	(0xf << (((dac) + 1) * 4))
#define SUNXI_TVE_GCTRL_DAC_INPUT(dac, sel)	((sel) << (((dac) + 1) * 4))
#define SUNXI_TVE_CFG0_VGA			0x20000000
#define SUNXI_TVE_CFG0_PAL			0x07030001
#define SUNXI_TVE_CFG0_NTSC			0x07030000
#define SUNXI_TVE_DAC_CFG0_VGA			0x403e1ac7
#ifdef CONFIG_MACH_SUN5I
#define SUNXI_TVE_DAC_CFG0_COMPOSITE		0x433f0009
#else
#define SUNXI_TVE_DAC_CFG0_COMPOSITE		0x403f0008
#endif
#define SUNXI_TVE_FILTER_COMPOSITE		0x00000120
#define SUNXI_TVE_CHROMA_FREQ_PAL_M		0x21e6efe3
#define SUNXI_TVE_CHROMA_FREQ_PAL_NC		0x21f69446
#define SUNXI_TVE_PORCH_NUM_PAL			0x008a0018
#define SUNXI_TVE_PORCH_NUM_NTSC		0x00760020
#define SUNXI_TVE_LINE_NUM_PAL			0x00160271
#define SUNXI_TVE_LINE_NUM_NTSC			0x0016020d
#define SUNXI_TVE_BLANK_BLACK_LEVEL_PAL		0x00fc00fc
#define SUNXI_TVE_BLANK_BLACK_LEVEL_NTSC	0x00f0011a
#define SUNXI_TVE_UNKNOWN1_VGA			0x00000000
#define SUNXI_TVE_UNKNOWN1_COMPOSITE		0x18181818
#define SUNXI_TVE_AUTO_DETECT_EN_DET_EN(dac)	(1 << ((dac) + 0))
#define SUNXI_TVE_AUTO_DETECT_EN_INT_EN(dac)	(1 << ((dac) + 16))
#define SUNXI_TVE_AUTO_DETECT_INT_STATUS(dac)	(1 << ((dac) + 0))
#define SUNXI_TVE_AUTO_DETECT_STATUS_SHIFT(dac)	((dac) * 8)
#define SUNXI_TVE_AUTO_DETECT_STATUS_MASK(dac)	(3 << ((dac) * 8))
#define SUNXI_TVE_AUTO_DETECT_STATUS_NONE	0
#define SUNXI_TVE_AUTO_DETECT_STATUS_CONNECTED	1
#define SUNXI_TVE_AUTO_DETECT_STATUS_SHORT_GND	3
#define SUNXI_TVE_AUTO_DETECT_DEBOUNCE_SHIFT(d)	((d) * 8)
#define SUNXI_TVE_AUTO_DETECT_DEBOUNCE_MASK(d)	(0xf << ((d) * 8))
#define SUNXI_TVE_CSC_REG0_ENABLE		(1 << 31)
#define SUNXI_TVE_CSC_REG0			0x08440832
#define SUNXI_TVE_CSC_REG1			0x3b6dace1
#define SUNXI_TVE_CSC_REG2			0x0e1d13dc
#define SUNXI_TVE_CSC_REG3			0x00108080
#define SUNXI_TVE_COLOR_BURST_PAL_M		0x00000000
#define SUNXI_TVE_CBR_LEVEL_PAL			0x00002828
#define SUNXI_TVE_CBR_LEVEL_NTSC		0x0000004f
#define SUNXI_TVE_BURST_PHASE_NTSC		0x00000000
#define SUNXI_TVE_BURST_WIDTH_COMPOSITE		0x0016447e
#define SUNXI_TVE_UNKNOWN2_PAL			0x0000e0e0
#define SUNXI_TVE_UNKNOWN2_NTSC			0x0000a0a0
#define SUNXI_TVE_SYNC_VBI_LEVEL_NTSC		0x001000f0
#define SUNXI_TVE_ACTIVE_NUM_COMPOSITE		0x000005a0
#define SUNXI_TVE_CHROMA_BW_GAIN_COMP		0x00000002
#define SUNXI_TVE_NOTCH_WIDTH_COMPOSITE		0x00000101
#define SUNXI_TVE_RESYNC_NUM_PAL		0x800d000c
#define SUNXI_TVE_RESYNC_NUM_NTSC		0x000e000c
#define SUNXI_TVE_SLAVE_PARA_COMPOSITE		0x00000000

void tvencoder_mode_set(struct sunxi_tve_reg * const tve, enum tve_mode mode);
void tvencoder_enable(struct sunxi_tve_reg * const tve);

typedef enum
{
    TVE_MODE_NTSC,
    TVE_MODE_PAL,
} tve_mode_e;

typedef enum
{
    DE_LCD,
    DE_TV,
} de_mode_e;

typedef enum
{
    TVE_ENABLE          = 0x000,
    TVE_CFG1            = 0x004,
    TVE_DAC1            = 0x008,
    TVE_NOTCH_DELAY     = 0x00C,
    TVE_CHROMA_FREQ     = 0x010,
    TVE_FB_PORCH        = 0x014,
    TVE_HD_VS           = 0x018,
    TVE_LINE_NUM        = 0x01C,
    TVE_LEVEL           = 0x020,
    TVE_DAC2            = 0x024,
    TVE_AUTO_EN         = 0x030,
    TVE_AUTO_ISR        = 0x034,
    TVE_AUTO_SR         = 0x038,
    TVE_AUTO_DEB        = 0x03C,
    TVE_CSC1            = 0x040,
    TVE_CSC2            = 0x044,
    TVE_CSC3            = 0x048,
    TVE_CSC4            = 0x04C,
    TVE_REG_0F8         = 0x0F8,
    TVE_REG_0FC         = 0x0FC,
    TVE_CB_RESET        = 0x100,
    TVE_VS_NUM          = 0x104,
    TVE_FILTER          = 0x108,
    TVE_CBCR_LEVEL      = 0x10C,
    TVE_TINT_PHASE      = 0x110,
    TVE_B_WIDTH         = 0x114,
    TVE_CBCR_GAIN       = 0x118,
    TVE_SYNC_LEVEL      = 0x11C,
    TVE_WHITE_LEVEL     = 0x120,
    TVE_ACT_LINE        = 0x124,
    TVE_CHROMA_BW       = 0x128,
    TVE_CFG2            = 0x12C,
    TVE_RESYNC          = 0x130,
    TVE_SLAVE           = 0x134,
    TVE_CFG3            = 0x138,
    TVE_CFG4            = 0x13C,
} tve_reg_e;

typedef enum
{
    DEBE_1BPP   = (1 << 8),
    DEBE_2BPP   = (2 << 8),
    DEBE_4BPP   = (4 << 8),
    DEBE_8BPP   = (8 << 8),
    DEBE_16BPP  = (16 << 8),
    DEBE_24BPP  = (24 << 8),
    DEBE_32BPP  = (32 << 8),
} debe_color_mode_bpp_e;

#define DEBE_PALETTE_EN 0x80

typedef enum
{
    DEBE_MODE__1BPP_MONO        = 0 | DEBE_1BPP,
    DEBE_MODE__2BPP_MONO        = 1 | DEBE_2BPP,
    DEBE_MODE__4BPP_MONO        = 2 | DEBE_4BPP,
    DEBE_MODE__8BPP_MONO        = 3 | DEBE_8BPP,
    DEBE_MODE_16BPP_RGB_655     = 4 | DEBE_16BPP,
    DEBE_MODE_16BPP_RGB_565     = 5 | DEBE_16BPP,
    DEBE_MODE_16BPP_RGB_556     = 6 | DEBE_16BPP,
    DEBE_MODE_16BPP_ARGB_1555   = 7 | DEBE_16BPP,
    DEBE_MODE_16BPP_RGBA_5551   = 8 | DEBE_16BPP,
    DEBE_MODE_32BPP_RGB_888     = 9 | DEBE_32BPP,
    DEBE_MODE_32BPP_ARGB_8888   = 10 | DEBE_32BPP,
    DEBE_MODE_24BPP_RGB_888     = 11 | DEBE_24BPP,
    DEBE_MODE__1BPP_PALETTE     = 0 | DEBE_PALETTE_EN | DEBE_1BPP,
    DEBE_MODE__2BPP_PALETTE     = 1 | DEBE_PALETTE_EN | DEBE_2BPP,
    DEBE_MODE__4BPP_PALETTE     = 2 | DEBE_PALETTE_EN | DEBE_4BPP,
    DEBE_MODE__8BPP_PALETTE     = 3 | DEBE_PALETTE_EN | DEBE_8BPP,
    DEBE_MODE_DEFE_VIDEO        = 0x40,
    DEBE_MODE_YUV               = 0x41,
} debe_color_mode_e;


typedef enum
{
    CLK_DE_SRC_PLL_VIDEO = 0,
    CLK_DE_SRC_PLL_PERIPH = 2,
} clk_source_de_e;
#endif /* _TVE_H */
