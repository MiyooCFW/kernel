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
#ifndef __SUNIV_CCM_H__
#define __SUNIV_CCM_H__

#define PLL_CPU_CTRL_REG          0x0000 //PLL_CPU Control Register
#define PLL_AUDIO_CTRL_REG        0x0008 //PLL_AUDIO Control Register
#define PLL_VIDEO_CTRL_REG        0x0010 //PLL_VIDEO Control Register
#define PLL_VE_CTRL_REG           0x0018 //PLL_VE Control Register
#define PLL_DDR_CTRL_REG          0x0020 //PLL_DDR Control Register
#define PLL_PERIPH_CTRL_REG       0x0028 //PLL_PERIPH Control Register
#define CPU_CLK_SRC_REG           0x0050 //CPU Clock Source Register
#define AHB_APB_HCLKC_CFG_REG     0x0054 //AHB/APB/HCLKC Configuration Register
#define BUS_CLK_GATING_REG0       0x0060 //Bus Clock Gating Register 0
#define BUS_CLK_GATING_REG1       0x0064 //Bus Clock Gating Register 1
#define BUS_CLK_GATING_REG2       0x0068 //Bus Clock Gating Register 2
#define SDMMC0_CLK_REG            0x0088 //SDMMC0 Clock Register
#define SDMMC1_CLK_REG            0x008C //SDMMC1 Clock Register
#define DAUDIO_CLK_REG            0x00B0 //DAUDIO Clock Register
#define OWA_CLK_REG               0x00B4 //OWA Clock Register
#define CIR_CLK_REG               0x00B8 //CIR Clock Register
#define USBPHY_CLK_REG            0x00CC //USBPHY Clock Register
#define DRAM_GATING_REG           0x0100 //DRAM GATING Register
#define BE_CLK_REG                0x0104 //BE Clock Register
#define FE_CLK_REG                0x010C //FE Clock Register
#define TCON_CLK_REG              0x0118 //TCON Clock Register
#define DI_CLK_REG                0x011C //De-interlacer Clock Register
#define TVE_CLK_REG               0x0120 //TVE Clock Register
#define TVD_CLK_REG               0x0124 //TVD Clock Register
#define CSI_CLK_REG               0x0134 //CSI Clock Register
#define VE_CLK_REG                0x013C //VE Clock Register
#define AUDIO_CODEC_CLK_REG       0x0140 //Audio Codec Clock Register
#define AVS_CLK_REG               0x0144 //AVS Clock Register
#define PLL_STABLE_TIME_REG0      0x0200 //PLL Stable Time Register 0
#define PLL_STABLE_TIME_REG1      0x0204 //PLL Stable Time Register 1
#define PLL_CPU_BIAS_REG          0x0220 //PLL_CPU Bias Register
#define PLL_AUDIO_BIAS_REG        0x0224 //PLL_AUDIO Bias Register
#define PLL_VIDEO_BIAS_REG        0x0228 //PLL_VIDEO Bias Register
#define PLL_VE_BIAS_REG           0x022C //PLL_VE Bias Register
#define PLL_DDR_BIAS_REG          0x0230 //PLL_DDR Bias Register
#define PLL_PERIPH_BIAS_REG       0x0234 //PLL_PERIPH Bias Register
#define PLL_CPU_TUN_REG           0x0250 //PLL_CPU Tuning Register
#define PLL_DDR_TUN_REG           0x0260 //PLL_DDR Tuning Register
#define PLL_AUDIO_PAT_CTRL_REG    0x0284 //PLL_AUDIO Pattern Control Register
#define PLL_VIDEO_PAT_CTRL_REG    0x0288 //PLL_VIDEO Pattern Control Register
#define PLL_DDR_PAT_CTRL_REG      0x0290 //PLL_DDR Pattern Control Register
#define BUS_SOFT_RST_REG0         0x02C0 //Bus Software Reset Register 0
#define BUS_SOFT_RST_REG1         0x02C4 //Bus Software Reset Register 1
#define BUS_SOFT_RST_REG2         0x02D0 //Bus Software Reset Register 2

typedef enum
{
    CCU_PLL_CPU_CTRL     = 0x000,
    CCU_PLL_AUDIO_CTRL   = 0x008,
    CCU_PLL_VIDEO_CTRL   = 0x010,
    CCU_PLL_VE_CTRL      = 0x018,
    CCU_PLL_DDR_CTRL     = 0x020,
    CCU_PLL_PERIPH_CTRL  = 0x028,
    CCU_CPU_CFG          = 0x050,
    CCU_AHB_APB_CFG      = 0x054,

    CCU_BUS_CLK_GATE0    = 0x060,
    CCU_BUS_CLK_GATE1    = 0x064,
    CCU_BUS_CLK_GATE2    = 0x068,

    CCU_SDMMC0_CLK       = 0x088,
    CCU_SDMMC1_CLK       = 0x08c,
    CCU_DAUDIO_CLK       = 0x0b0,
    CCU_SPDIF_CLK        = 0x0b4,
    CCU_I2S_CLK          = 0x0b8,
    CCU_USBPHY_CFG       = 0x0cc,
    CCU_DRAM_CLK_GATE    = 0x100,
    CCU_DEBE_CLK         = 0x104,
    CCU_DEFE_CLK         = 0x10c,
    CCU_TCON_CLK         = 0x118,
    CCU_DEINTERLACE_CLK  = 0x11c,
    CCU_TVE_CLK          = 0x120,
    CCU_TVD_CLK          = 0x124,
    CCU_CSI_CLK          = 0x134,
    CCU_VE_CLK           = 0x13c,
    CCU_ADDA_CLK         = 0x140,
    CCU_AVS_CLK          = 0x144,

    CCU_PLL_STABLE_TIME0 = 0x200,
    CCU_PLL_STABLE_TIME1 = 0x204,
    CCU_PLL_CPU_BIAS     = 0x220,
    CCU_PLL_AUDIO_BIAS   = 0x224,
    CCU_PLL_VIDEO_BIAS   = 0x228,
    CCU_PLL_VE_BIAS      = 0x22c,
    CCU_PLL_DDR0_BIAS    = 0x230,
    CCU_PLL_PERIPH_BIAS  = 0x234,
    CCU_PLL_CPU_TUN      = 0x250,
    CCU_PLL_DDR_TUN      = 0x260,
    CCU_PLL_AUDIO_PAT    = 0x284,
    CCU_PLL_VIDEO_PAT    = 0x288,
    CCU_PLL_DDR0_PAT     = 0x290,

    CCU_BUS_SOFT_RST0    = 0x2c0,
    CCU_BUS_SOFT_RST1    = 0x2c4,
    CCU_BUS_SOFT_RST2    = 0x2d0,
} ccu_reg_e;

typedef enum
{
    CLK_CPU_SRC_LOSC = 0, // not used?
    CLK_CPU_SRC_OSC24M = 1,
    CLK_CPU_SRC_PLL_CPU = 2,
} clk_source_cpu_e;

typedef enum
{
    PLL_CPU = CCU_PLL_CPU_CTRL,
    PLL_AUDIO = CCU_PLL_AUDIO_CTRL,
    PLL_VIDEO = CCU_PLL_VIDEO_CTRL,
    PLL_VE = CCU_PLL_VE_CTRL,
    PLL_DDR = CCU_PLL_DDR_CTRL,
    PLL_PERIPH = CCU_PLL_PERIPH_CTRL,
} pll_ch_e;

typedef enum
{
    CLK_AHB_SRC_LOSC = 0,
    CLK_AHB_SRC_OSC24M = 1,
    CLK_AHB_SRC_CPUCLK = 2,
    CLK_AHB_SRC_PLL_PERIPH_PREDIV = 3,
} clk_source_ahb_e;

typedef enum
{
    CLK_APB_DIV_2 = 1,
    CLK_APB_DIV_4 = 2,
    CLK_APB_DIV_8 = 3,
} clk_div_apb_e;

#endif

