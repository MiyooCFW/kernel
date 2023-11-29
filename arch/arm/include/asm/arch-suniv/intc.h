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
#ifndef __SUNIV_INTC_H__
#define __SUNIV_INTC_H__

#define INTC_VECTOR_REG     0x00 //Interrupt Vector Register
#define INTC_BASE_ADDR_REG  0x04 //Interrupt Base Address Register
#define NMI_INT_CTRL_REG    0x0C //NMI Interrupt Control Register
#define INTC_PEND_REG0      0x10 //Interrupt Pending Register 0
#define INTC_PEND_REG1      0x14 //Interrupt Pending Register 1
#define INTC_EN_REG0        0x20 //Interrupt Enable Register 0
#define INTC_EN_REG1        0x24 //Interrupt Enable Register 1
#define INTC_MASK_REG0      0x30 //Interrupt Mask Register 0
#define INTC_MASK_REG1      0x34 //Interrupt Mask Register 1
#define INTC_RESP_REG0      0x40 //Interrupt Response Register 0
#define INTC_RESP_REG1      0x44 //Interrupt Response Register 1
#define INTC_FF_REG0        0x50 //Interrupt Fast Forcing Register 0
#define INTC_FF_REG1        0x54 //Interrupt Fast Forcing Register 1
#define INTC_PRIO_REG0      0x60 //Interrupt Source Priority Register 0
#define INTC_PRIO_REG1      0x64 //Interrupt Source Priority Register 1
#define INTC_PRIO_REG2      0x68 //Interrupt Source Priority Register 2
#define INTC_PRIO_REG3      0x6C //Interrupt Source Priority Register 3

#endif

