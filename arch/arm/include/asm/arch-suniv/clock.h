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

#endif

