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
#ifndef __SUNIV_CODEC_H__
#define __SUNIV_CODEC_H__

#define AC_DAC_DPC_REG            0x00 //DAC Digital Part Control Register
#define AC_DAC_FIFOC_REG          0x04 //DAC FIFO Control Register
#define AC_DAC_FIFOS_REG          0x08 //DAC FIFO Status Register
#define AC_DAC_TXDATA_REG         0x0c //DAC TX Data Register
#define AC_ADC_FIFOC_REG          0x10 //ADC FIFO Control Register
#define AC_ADC_FIFOS_REG          0x14 //ADC FIFO Status Register
#define AC_ADC_RXDATA_REG         0x18 //ADC RX Data Register
#define DAC_MIXER_CTRL_REG        0x20 //DAC&MIXER Control Register
#define ADC_MIXER_CTRL_REG        0x24 //ADC Analog and Input mixer Control Register
#define ADDA_TUNE_REG             0x28 //ADC&DAC performance tuning Register
#define BIAS_DA16_CAL_CTRL_REG0   0x2C //Bias&DA16 Calibration Control Register 0
#define BIAS_DA16_CAL_CTRL_REG1   0x34 //Bias&DA16 Calibration Control Register 1
#define AC_DAC_CNT_REG            0x40 //DAC TX FIFO Counter Register
#define AC_ADC_CNT_REG            0x44 //ADC RX FIFO Counter Register
#define AC_DAC_DG_REG             0x48 //DAC Debug Register
#define AC_ADC_DG_REG             0x4c //ADC Debug Register
#define AC_ADC_DAP_CTR_REG        0x70 //ADC DAP Control Register
#define AC_ADC_DAP_LCTR_REG       0x74 //ADC DAP Left Control Register
#define AC_ADC_DAP_RCTR_REG       0x78 //ADC DAP Right Control Register
#define AC_ADC_DAP_PARA_REG       0x7C //ADC DAP Parameter Register
#define AC_ADC_DAP_LAC_REG        0x80 //ADC DAP Left Average Coef Register
#define AC_ADC_DAP_LDAT_REG       0x84 //ADC DAP Left Decay and Attack Time Register
#define AC_ADC_DAP_RAC_REG        0x88 //ADC DAP Right Average Coef Register
#define AC_ADC_DAP_RDAT_REG       0x8C //ADC DAP Right Decay and Attack Time Register
#define ADC_DAP_HPFC_REG          0x90 //ADC DAP HPF Coef Register
#define ADC_DAP_LINAC_REG         0x94 //ADC DAP Left Input Signal Low Average Coef Register
#define ADC_DAP_RINAC_REG         0x98 //ADC DAP Right Input Signal Low Average Coef Register
#define ADC_DAP_ORT_REG           0x9c //ADC DAP Optimum Register

#endif

