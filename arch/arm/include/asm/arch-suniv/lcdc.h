/*
 * Copyright (C) 2018 Steward Fu <steward.fu@gmail.com>
 *
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
#ifndef __SUNIV_LCDC_H__
#define __SUNIV_LCDC_H__

#define TCON_CTRL_REG             0x000 //TCON Control Register
#define TCON_INT_REG0             0x004 //TCON Interrupt Register 0
#define TCON_INT_REG1             0x008 //TCON Interrupt Register 1
#define TCON_FRM_CTRL_REG         0x010 //TCON FRM Control Register
#define TCON_FRM_SEED0_R_REG      0x014 //TCON FRM Seed0 Red Register
#define TCON_FRM_SEED0_G_REG      0x018 //TCON FRM Seed0 Green Register
#define TCON_FRM_SEED0_B_REG      0x01C //TCON FRM Seed0 Blue Register
#define TCON_FRM_SEED1_R_REG      0x020 //TCON FRM Seed1 Red Register
#define TCON_FRM_SEED1_G_REG      0x024 //TCON FRM Seed1 Green Register
#define TCON_FRM_SEED1_B_REG      0x028 //TCON FRM Seed1 Blue Register
#define TCON_FRM_TBL_REG0         0x02C //TCON FRM Table Register 0
#define TCON_FRM_TBL_REG1         0x030 //TCON FRM Table Register 1
#define TCON_FRM_TBL_REG2         0x034 //TCON FRM Table Register 2
#define TCON_FRM_TBL_REG3         0x038 //TCON FRM Table Register 3
#define TCON0_CTRL_REG            0x040 //TCON0 Control Register
#define TCON_CLK_CTRL_REG         0x044 //TCON Clock Control Register
#define TCON0_BASIC_TIMING_REG0   0x048 //TCON0 Basic Timing Register 0
#define TCON0_BASIC_TIMING_REG1   0x04C //TCON0 Basic Timing Register 1
#define TCON0_BASIC_TIMING_REG2   0x050 //TCON0 Basic Timing Register 2
#define TCON0_BASIC_TIMING_REG3   0x054 //TCON0 Basic Timing Register 3
#define TCON0_HV_TIMING_REG       0x058 //TCON0 HV Timing Register
#define TCON0_CPU_IF_REG          0x060 //TCON0 CPU Interface Control Register
#define TCON0_CPU_WR_REG          0x064 //TCON0 CPU Mode Write Register
#define TCON0_CPU_RD_REG          0x068 //TCON0 CPU Mode Read Register
#define TCON0_CPU_RD_NX_REG       0x06C //TCON0 CPU Mode Read NX Register
#define TCON0_IO_CTRL_REG0        0x088 //TCON0 IO Control Register 0
#define TCON0_IO_CTRL_REG1        0x08C //TCON0 IO Control Register 1
#define TCON1_CTRL_REG            0x090 //TCON1 Control Register
#define TCON1_BASIC_REG0          0x094 //TCON1 Basic Timing Register 0
#define TCON1_BASIC_REG1          0x098 //TCON1 Basic Timing Register 1
#define TCON1_BASIC_REG2          0x09C //TCON1 Basic Timing Register 2
#define TCON1_BASIC_REG3          0x0A0 //TCON1 Basic Timing Register 3
#define TCON1_BASIC_REG4          0x0A4 //TCON1 Basic Timing Register 4
#define TCON1_BASIC_REG5          0x0A8 //TCON1 Basic Timing Register 5
#define TCON1_IO_CTRL_REG0        0x0F0 //TCON1 IO Control Register 0
#define TCON1_IO_CTRL_REG1        0x0F4 //TCON1 IO Control Register 1
#define TCON_DEBUG_INFO_REG       0x0FC //TCON Debug Information Register

#endif

