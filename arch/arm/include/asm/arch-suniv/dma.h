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
#ifndef __SUNIV_DMA_H__
#define __SUNIV_DMA_H__

#define DMA_INT_CTRL_REG      0x00                  //DMA Interrupt Control Register
#define DMA_INT_STA_REG       0x04                  //DMA Interrupt Status Register
#define DMA_PTY_CFG_REG       0x08                  //DMA Priority Configure Register
#define NDMA0_CFG_REG         (0x100+(0*0x20)+0x0)  //Normal DMA Configure Register n (n=0~3)
#define NDMA0_SRC_ADR_REG     (0x100+(0*0x20)+0x4)  //Normal DMA Source Address Register n (n=0~3)
#define NDMA0_DES_ADR_REG     (0x100+(0*0x20)+0x8)  //Normal DMA Destination Address Register n (n=0~3)
#define NDMA0_BYTE_CNT_REG    (0x100+(0*0x20)+0xC)  //Normal DMA Byte Counter Register n (n=0~3)
#define NDMA1_CFG_REG         (0x100+(1*0x20)+0x0)  //Normal DMA Configure Register n (n=0~3)
#define NDMA1_SRC_ADR_REG     (0x100+(1*0x20)+0x4)  //Normal DMA Source Address Register n (n=0~3)
#define NDMA1_DES_ADR_REG     (0x100+(1*0x20)+0x8)  //Normal DMA Destination Address Register n (n=0~3)
#define NDMA1_BYTE_CNT_REG    (0x100+(1*0x20)+0xC)  //Normal DMA Byte Counter Register n (n=0~3)
#define NDMA2_CFG_REG         (0x100+(2*0x20)+0x0)  //Normal DMA Configure Register n (n=0~3)
#define NDMA2_SRC_ADR_REG     (0x100+(2*0x20)+0x4)  //Normal DMA Source Address Register n (n=0~3)
#define NDMA2_DES_ADR_REG     (0x100+(2*0x20)+0x8)  //Normal DMA Destination Address Register n (n=0~3)
#define NDMA2_BYTE_CNT_REG    (0x100+(2*0x20)+0xC)  //Normal DMA Byte Counter Register n (n=0~3)
#define NDMA3_CFG_REG         (0x100+(3*0x20)+0x0)  //Normal DMA Configure Register n (n=0~3)
#define NDMA3_SRC_ADR_REG     (0x100+(3*0x20)+0x4)  //Normal DMA Source Address Register n (n=0~3)
#define NDMA3_DES_ADR_REG     (0x100+(3*0x20)+0x8)  //Normal DMA Destination Address Register n (n=0~3)
#define NDMA3_BYTE_CNT_REG    (0x100+(3*0x20)+0xC)  //Normal DMA Byte Counter Register n (n=0~3)


#define DDMA0_CFG_REG         (0x300+(0*0x20)+0x0)  //Dedicated DMA Configure Register n (n=0~3)
#define DDMA0_SRC_ADR_REG     (0x300+(0*0x20)+0x4)  //Dedicated DMA Source Address Register n (n=0~3)
#define DDMA0_DES_ADR_REG     (0x300+(0*0x20)+0x8)  //Dedicated DMA Destination Address Register n (n=0~3)
#define DDMA0_BYTE_CNT_REG    (0x300+(0*0x20)+0xC)  //Dedicated DMA Byte Counter Register n (n=0~3)
#define DDMA0_PAR_REG         (0x300+(0*0x20)+0x18) //Dedicated DMA Parameter Register n (n=0~3)
#define DDMA0_GEN_DATA        (0x300+(0*0x20)+0x1c) //Dedicated DMA General DATA Register 3
#define DDMA1_CFG_REG         (0x300+(1*0x20)+0x0)  //Dedicated DMA Configure Register n (n=0~3)
#define DDMA1_SRC_ADR_REG     (0x300+(1*0x20)+0x4)  //Dedicated DMA Source Address Register n (n=0~3)
#define DDMA1_DES_ADR_REG     (0x300+(1*0x20)+0x8)  //Dedicated DMA Destination Address Register n (n=0~3)
#define DDMA1_BYTE_CNT_REG    (0x300+(1*0x20)+0xC)  //Dedicated DMA Byte Counter Register n (n=0~3)
#define DDMA1_PAR_REG         (0x300+(1*0x20)+0x18) //Dedicated DMA Parameter Register n (n=0~3)
#define DDMA1_GEN_DATA        (0x300+(1*0x20)+0x1c) //Dedicated DMA General DATA Register 3
#define DDMA2_CFG_REG         (0x300+(2*0x20)+0x0)  //Dedicated DMA Configure Register n (n=0~3)
#define DDMA2_SRC_ADR_REG     (0x300+(2*0x20)+0x4)  //Dedicated DMA Source Address Register n (n=0~3)
#define DDMA2_DES_ADR_REG     (0x300+(2*0x20)+0x8)  //Dedicated DMA Destination Address Register n (n=0~3)
#define DDMA2_BYTE_CNT_REG    (0x300+(2*0x20)+0xC)  //Dedicated DMA Byte Counter Register n (n=0~3)
#define DDMA2_PAR_REG         (0x300+(2*0x20)+0x18) //Dedicated DMA Parameter Register n (n=0~3)
#define DDMA2_GEN_DATA        (0x300+(2*0x20)+0x1c) //Dedicated DMA General DATA Register 3
#define DDMA3_CFG_REG         (0x300+(3*0x20)+0x0)  //Dedicated DMA Configure Register n (n=0~3)
#define DDMA3_SRC_ADR_REG     (0x300+(3*0x20)+0x4)  //Dedicated DMA Source Address Register n (n=0~3)
#define DDMA3_DES_ADR_REG     (0x300+(3*0x20)+0x8)  //Dedicated DMA Destination Address Register n (n=0~3)
#define DDMA3_BYTE_CNT_REG    (0x300+(3*0x20)+0xC)  //Dedicated DMA Byte Counter Register n (n=0~3)
#define DDMA3_PAR_REG         (0x300+(3*0x20)+0x18) //Dedicated DMA Parameter Register n (n=0~3)
#define DDMA3_GEN_DATA        (0x300+(3*0x20)+0x1c) //Dedicated DMA General DATA Register 3

#endif
