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
#ifndef __SUNIV_DEBE_H__
#define __SUNIV_DEBE_H__

#define DEBE_MODE_CTRL_REG        0x800 //DEBE Mode Control Register
#define DEBE_COLOR_CTRL_REG       0x804 //DEBE Color Control Register
#define DEBE_DISP_SIZE_REG        0x808 //DEBE Display Size (Undocument)
#define DEBE_LAY0_SIZE_REG        0x810 //DEBE Layer 0 Size Register
#define DEBE_LAY1_SIZE_REG        0x814 //DEBE Layer 1 Size Register
#define DEBE_LAY2_SIZE_REG        0x818 //DEBE Layer 2 Size Register
#define DEBE_LAY3_SIZE_REG        0x81C //DEBE Layer 3 Size Register
#define DEBE_LAY0_CODNT_REG       0x820 //DEBE layer 0 Coordinate Control Register
#define DEBE_LAY1_CODNT_REG       0x824 //DEBE Layer 1 Coordinate Control Register
#define DEBE_LAY2_CODNT_REG       0x828 //DEBE Layer 2 Coordinate Control Register
#define DEBE_LAY3_CODNT_REG       0x82C //DEBE Layer 3 Coordinate Control Register
#define DEBE_LAY0_LINEWIDTH_REG   0x840 //DEBE Layer 0 Frame Buffer Line Width Register
#define DEBE_LAY1_LINEWIDTH_REG   0x844 //DEBE Layer 1 Frame Buffer Line Width Register
#define DEBE_LAY2_LINEWIDTH_REG   0x848 //DEBE Layer 2 Frame Buffer Line Width Register
#define DEBE_LAY3_LINEWIDTH_REG   0x84C //DEBE Layer 3 Frame Buffer Line Width Register
#define DEBE_LAY0_FB_ADDR_REG     0x850 //DEBE Layer 0 Frame Buffer Address Register
#define DEBE_LAY1_FB_ADDR_REG     0x854 //DEBE Layer 1 Frame Buffer Address Register
#define DEBE_LAY2_FB_ADDR_REG     0x858 //DEBE Layer 2 Frame Buffer Address Register
#define DEBE_LAY3_FB_ADDR_REG     0x85C //DEBE Layer 3 Frame Buffer Address Register
#define DEBE_LAY0_FB_HI_ADDR_REG  0x860 //DEBE Layer 0 Frame Buffer High Address Register (Undocument)
#define DEBE_LAY1_FB_HI_ADDR_REG  0x864 //DEBE Layer 1 Frame Buffer High Address Register (Undocument)
#define DEBE_LAY2_FB_HI_ADDR_REG  0x868 //DEBE Layer 2 Frame Buffer High Address Register (Undocument)
#define DEBE_LAY3_FB_HI_ADDR_REG  0x86c //DEBE Layer 3 Frame Buffer High Address Register (Undocument)
#define DEBE_REGBUFF_CTRL_REG     0x870 //DEBE Register Buffer Control Register
#define DEBE_CK_MAX_REG           0x880 //DEBE Color Key Max Register
#define DEBE_CK_MIN_REG           0x884 //DEBE Color Key Min register
#define DEBE_CK_CFG_REG           0x888 //DEBE Color Key Configuration Register
#define DEBE_LAY0_ATT_CTRL_REG0   0x890 //DEBE Layer 0 Attribute Control Register 0
#define DEBE_LAY1_ATT_CTRL_REG0   0x894 //DEBE Layer 1 Attribute Control register 1
#define DEBE_LAY2_ATT_CTRL_REG0   0x898 //DEBE Layer2 Attribute Control Register 0
#define DEBE_LAY3_ATT_CTRL_REG0   0x89C //DEBE Layer3 Attribute Control Register 0
#define DEBE_LAY0_ATT_CTRL_REG1   0x8A0 //DEBE Layer0 Attribute Control Register 1
#define DEBE_LAY1_ATT_CTRL_REG1   0x8A4 //DEBE Layer 1 Attribute Control Register 1
#define DEBE_LAY2_ATT_CTRL_REG1   0x8A8 //DEBE Layer 2 Attribute Control Register 1
#define DEBE_LAY3_ATT_CTRL_REG1   0x8AC //DEBE Layer 3 Attribute Control Register 1
#define DEBE_HWC_CTRL_REG         0x8D8 //DEBE HWC Coordinate Control Register
#define DEBE_HWCFB_CTRL_REG       0x8E0 //DEBE HWC Frame Buffer Format Register
#define DEBE_WB_CTRL_REG          0x8F0 //DEBE Write Back Control Register
#define DEBE_WB_ADDR_REG          0x8F4 //DEBE Write Back Address Register
#define DEBE_WB_LW_REG            0x8F8 //DEBE Write Back Buffer Line Width Register
#define DEBE_IYUV_CH_CTRL_REG     0x920 //DEBE Input YUV Channel Control Register
#define DEBE_CH0_YUV_FB_ADDR_REG  0x930 //DEBE YUV Channel 0 Frame Buffer Address Register
#define DEBE_CH1_YUV_FB_ADDR_REG  0x934 //DEBE YUV Channel 1 Frame Buffer Address Register
#define DEBE_CH2_YUV_FB_ADDR_REG  0x938 //DEBE YUV Channel 2 Frame Buffer Address Register
#define DEBE_CH0_YUV_BLW_REG      0x940 //DEBE YUV Channel 0 Buffer Line Width Register
#define DEBE_CH1_YUV_BLW_REG      0x944 //DEBE YUV Channel 1 Buffer Line Width Register
#define DEBE_CH2_YUV_BLW_REG      0x948 //DEBE YUV Channel 2 Buffer Line Width Register
#define DEBE_COEF00_REG           0x950 //DEBE Coefficient 00 Register
#define DEBE_COEF01_REG           0x954 //DEBE Coefficient 01 Register
#define DEBE_COEF02_REG           0x958 //DEBE Coefficient 02 Register
#define DEBE_COEF03_REG           0x95C //DEBE Coefficient 03 Register
#define DEBE_COEF10_REG           0x960 //DEBE Coefficient 10 Register
#define DEBE_COEF11_REG           0x964 //DEBE Coefficient 11 Register
#define DEBE_COEF12_REG           0x968 //DEBE Coefficient 12 Register
#define DEBE_COEF13_REG           0x96C //DEBE Coefficient 13 Register
#define DEBE_COEF20_REG           0x970 //DEBE Coefficient 20 Register
#define DEBE_COEF21_REG           0x974 //DEBE Coefficient 21 Register
#define DEBE_COEF22_REG           0x978 //DEBE Coefficient 22 Register
#define DEBE_COEF23_REG           0x97C //DEBE Coefficient 23 Register

#endif

