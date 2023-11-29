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
#ifndef __SUNIV_GPIO_H__
#define __SUNIV_GPIO_H__

#define PA_CFG0     ((0*0x24)+0x00)       //Port n Configure Register 0 (n=0~5)
#define PA_CFG1     ((0*0x24)+0x04)       //Port n Configure Register 1 (n=0~5)
#define PA_CFG2     ((0*0x24)+0x08)       //Port n Configure Register 2 (n=0~5)
#define PA_CFG3     ((0*0x24)+0x0C)       //Port n Configure Register 3 (n=0~5)
#define PA_DATA     ((0*0x24)+0x10)       //Port n Data Register (n=0~5)
#define PA_DRV0     ((0*0x24)+0x14)       //Port n Multi-Driving Register 0 (n=0~5)
#define PA_DRV1     ((0*0x24)+0x18)       //Port n Multi-Driving Register 1 (n=0~5)
#define PA_PUL0     ((0*0x24)+0x1C)       //Port n Pull Register 0 (n=0~5)
#define PA_PUL1     ((0*0x24)+0x20)       //Port n Pull Register 1 (n=0~5)

#define PC_CFG0     ((2*0x24)+0x00)       //Port n Configure Register 0 (n=0~5)
#define PC_CFG1     ((2*0x24)+0x04)       //Port n Configure Register 1 (n=0~5)
#define PC_CFG2     ((2*0x24)+0x08)       //Port n Configure Register 2 (n=0~5)
#define PC_CFG3     ((2*0x24)+0x0C)       //Port n Configure Register 3 (n=0~5)
#define PC_DATA     ((2*0x24)+0x10)       //Port n Data Register (n=0~5)
#define PC_DRV0     ((2*0x24)+0x14)       //Port n Multi-Driving Register 0 (n=0~5)
#define PC_DRV1     ((2*0x24)+0x18)       //Port n Multi-Driving Register 1 (n=0~5)
#define PC_PUL0     ((2*0x24)+0x1C)       //Port n Pull Register 0 (n=0~5)
#define PC_PUL1     ((2*0x24)+0x20)       //Port n Pull Register 1 (n=0~5)

#define PD_CFG0     ((3*0x24)+0x00)       //Port n Configure Register 0 (n=0~5)
#define PD_CFG1     ((3*0x24)+0x04)       //Port n Configure Register 1 (n=0~5)
#define PD_CFG2     ((3*0x24)+0x08)       //Port n Configure Register 2 (n=0~5)
#define PD_CFG3     ((3*0x24)+0x0C)       //Port n Configure Register 3 (n=0~5)
#define PD_DATA     ((3*0x24)+0x10)       //Port n Data Register (n=0~5)
#define PD_DRV0     ((3*0x24)+0x14)       //Port n Multi-Driving Register 0 (n=0~5)
#define PD_DRV1     ((3*0x24)+0x18)       //Port n Multi-Driving Register 1 (n=0~5)
#define PD_PUL0     ((3*0x24)+0x1C)       //Port n Pull Register 0 (n=0~5)
#define PD_PUL1     ((3*0x24)+0x20)       //Port n Pull Register 1 (n=0~5)

#define PE_CFG0     ((4*0x24)+0x00)       //Port n Configure Register 0 (n=0~5)
#define PE_CFG1     ((4*0x24)+0x04)       //Port n Configure Register 1 (n=0~5)
#define PE_CFG2     ((4*0x24)+0x08)       //Port n Configure Register 2 (n=0~5)
#define PE_CFG3     ((4*0x24)+0x0C)       //Port n Configure Register 3 (n=0~5)
#define PE_DATA     ((4*0x24)+0x10)       //Port n Data Register (n=0~5)
#define PE_DRV0     ((4*0x24)+0x14)       //Port n Multi-Driving Register 0 (n=0~5)
#define PE_DRV1     ((4*0x24)+0x18)       //Port n Multi-Driving Register 1 (n=0~5)
#define PE_PUL0     ((4*0x24)+0x1C)       //Port n Pull Register 0 (n=0~5)
#define PE_PUL1     ((4*0x24)+0x20)       //Port n Pull Register 1 (n=0~5)

#define PD_INT_CFG0 (0x200+(3*0x20)+0x0)  //PIO Interrupt Configure Register 0 (n=0~2)
#define PD_INT_CFG1 (0x200+(3*0x20)+0x4)  //PIO Interrupt Configure Register 1 (n=0~2)
#define PD_INT_CFG2 (0x200+(3*0x20)+0x8)  //PIO Interrupt Configure Register 2 (n=0~2)
#define PD_INT_CFG3 (0x200+(3*0x20)+0xC)  //PIO Interrupt Configure Register 3 (n=0~2)
#define PD_INT_CTRL (0x200+(3*0x20)+0x10) //PIO Interrupt Control Register (n=0~2)
#define PD_INT_STA  (0x200+(3*0x20)+0x14) //PIO Interrupt Status Register (n=0~2)
#define PD_INT_DEB  (0x200+(3*0x20)+0x18) //PIO Interrupt Debounce Register (n=0~2)

#define PE_INT_CFG0 (0x200+(4*0x20)+0x0)  //PIO Interrupt Configure Register 0 (n=0~2)
#define PE_INT_CFG1 (0x200+(4*0x20)+0x4)  //PIO Interrupt Configure Register 1 (n=0~2)
#define PE_INT_CFG2 (0x200+(4*0x20)+0x8)  //PIO Interrupt Configure Register 2 (n=0~2)
#define PE_INT_CFG3 (0x200+(4*0x20)+0xC)  //PIO Interrupt Configure Register 3 (n=0~2)
#define PE_INT_CTRL (0x200+(4*0x20)+0x10) //PIO Interrupt Control Register (n=0~2)
#define PE_INT_STA  (0x200+(4*0x20)+0x14) //PIO Interrupt Status Register (n=0~2)
#define PE_INT_DEB  (0x200+(4*0x20)+0x18) //PIO Interrupt Debounce Register (n=0~2)

#define SDR_PAD_DRV 0x2C0                 //SDRAM Pad Multi-Driving Register
#define SDR_PAD_PUL 0x2C4                 //SDRAM Pad Pull Register

#endif

