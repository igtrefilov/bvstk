/*
 * (c) 2026, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * $QNXLicenseC:
 * Copyright 2007, QNX Software Systems. All Rights Reserved.
 *
 * You must obtain a written license from and pay applicable license fees to QNX
 * Software Systems before you may reproduce, modify or distribute this software,
 * or any work that includes all or part of this software.   Free development
 * licenses are available for evaluation and non-commercial purposes.  For more
 * information visit http://licensing.qnx.com or email licensing@qnx.com.
 *
 * This file may contain contributions from others.  Please review this entire
 * file for other proprietary rights or license notices, as well as the QNX
 * Development Suite License Guide at http://licensing.qnx.com/license-guide/
 * for other information.
 * $
 */

#ifndef __HW_XADC_H__
#define __HW_XADC_H__

#include <stdint.h>

/* Internal Register offsets of the XADC
 * The following constants provide access to each of the internal registers of
 * the XADC device.
 */

/* XADC Internal Channel Registers */
#define ZYNQ7000_XADC_TEMP_OFFSET                   0x00 /* On-chip Temperature Reg */
#define ZYNQ7000_XADC_VCCINT_OFFSET                 0x01 /* On-chip VCCINT Data Reg */
#define ZYNQ7000_XADC_VCCAUX_OFFSET                 0x02 /* On-chip VCCAUX Data Reg */
#define ZYNQ7000_XADC_VPVN_OFFSET                   0x03 /* ADC out of VP/VN       */
#define ZYNQ7000_XADC_VREFP_OFFSET                  0x04 /* On-chip VREFP Data Reg */
#define ZYNQ7000_XADC_VREFN_OFFSET                  0x05 /* On-chip VREFN Data Reg */
#define ZYNQ7000_XADC_VBRAM_OFFSET                  0x06 /* On-chip VBRAM , 7 Series */
#define ZYNQ7000_XADC_ADC_A_SUPPLY_CALIB_OFFSET     0x08 /* ADC A Supply Offset Reg */
#define ZYNQ7000_XADC_ADC_A_OFFSET_CALIB_OFFSET     0x09 /* ADC A Offset Data Reg */
#define ZYNQ7000_XADC_ADC_A_GAINERR_CALIB_OFFSET    0x0A /* ADC A Gain Error Reg  */
#define ZYNQ7000_XADC_VCCPINT_OFFSET                0x0D /* On-chip VCCPINT Reg, Zynq */
#define ZYNQ7000_XADC_VCCPAUX_OFFSET                0x0E /* On-chip VCCPAUX Reg, Zynq */
#define ZYNQ7000_XADC_VCCPDRO_OFFSET                0x0F /* On-chip VCCPDRO Reg, Zynq */

/* XADC External Channel Registers */
#define ZYNQ7000_XADC_AUX00_OFFSET                  0x10 /* ADC out of VAUXP0/VAUXN0 */
#define ZYNQ7000_XADC_AUX01_OFFSET                  0x11 /* ADC out of VAUXP1/VAUXN1 */
#define ZYNQ7000_XADC_AUX02_OFFSET                  0x12 /* ADC out of VAUXP2/VAUXN2 */
#define ZYNQ7000_XADC_AUX03_OFFSET                  0x13 /* ADC out of VAUXP3/VAUXN3 */
#define ZYNQ7000_XADC_AUX04_OFFSET                  0x14 /* ADC out of VAUXP4/VAUXN4 */
#define ZYNQ7000_XADC_AUX05_OFFSET                  0x15 /* ADC out of VAUXP5/VAUXN5 */
#define ZYNQ7000_XADC_AUX06_OFFSET                  0x16 /* ADC out of VAUXP6/VAUXN6 */
#define ZYNQ7000_XADC_AUX07_OFFSET                  0x17 /* ADC out of VAUXP7/VAUXN7 */
#define ZYNQ7000_XADC_AUX08_OFFSET                  0x18 /* ADC out of VAUXP8/VAUXN8 */
#define ZYNQ7000_XADC_AUX09_OFFSET                  0x19 /* ADC out of VAUXP9/VAUXN9 */
#define ZYNQ7000_XADC_AUX10_OFFSET                  0x1A /* ADC out of VAUXP10/VAUXN10 */
#define ZYNQ7000_XADC_AUX11_OFFSET                  0x1B /* ADC out of VAUXP11/VAUXN11 */
#define ZYNQ7000_XADC_AUX12_OFFSET                  0x1C /* ADC out of VAUXP12/VAUXN12 */
#define ZYNQ7000_XADC_AUX13_OFFSET                  0x1D /* ADC out of VAUXP13/VAUXN13 */
#define ZYNQ7000_XADC_AUX14_OFFSET                  0x1E /* ADC out of VAUXP14/VAUXN14 */
#define ZYNQ7000_XADC_AUX15_OFFSET                  0x1F /* ADC out of VAUXP15/VAUXN15 */

/*
 * XADC Registers for Maximum/Minimum data captured for the
 * on chip Temperature/VCCINT/VCCAUX data.
 */
#define ZYNQ7000_XADC_MAX_TEMP_OFFSET               0x20 /* Max Temperature Reg */
#define ZYNQ7000_XADC_MAX_VCCINT_OFFSET             0x21 /* Max VCCINT Register */
#define ZYNQ7000_XADC_MAX_VCCAUX_OFFSET             0x22 /* Max VCCAUX Register */
#define ZYNQ7000_XADC_MAX_VCCBRAM_OFFSET            0x23 /* Max BRAM Register, 7 series */
#define ZYNQ7000_XADC_MIN_TEMP_OFFSET               0x24 /* Min Temperature Reg */
#define ZYNQ7000_XADC_MIN_VCCINT_OFFSET             0x25 /* Min VCCINT Register */
#define ZYNQ7000_XADC_MIN_VCCAUX_OFFSET             0x26 /* Min VCCAUX Register */
#define ZYNQ7000_XADC_MIN_VCCBRAM_OFFSET            0x27 /* Min BRAM Register, 7 series */
#define ZYNQ7000_XADC_MAX_VCCPINT_OFFSET            0x28 /* Max VCCPINT Register, Zynq */
#define ZYNQ7000_XADC_MAX_VCCPAUX_OFFSET            0x29 /* Max VCCPAUX Register, Zynq */
#define ZYNQ7000_XADC_MAX_VCCPDRO_OFFSET            0x2A /* Max VCCPDRO Register, Zynq */
#define ZYNQ7000_XADC_MIN_VCCPINT_OFFSET            0x2C /* Min VCCPINT Register, Zynq */
#define ZYNQ7000_XADC_MIN_VCCPAUX_OFFSET            0x2D /* Min VCCPAUX Register, Zynq */
#define ZYNQ7000_XADC_MIN_VCCPDRO_OFFSET            0x2E /* Min VCCPDRO Register,Zynq */
#define ZYNQ7000_XADC_FLAG_OFFSET                   0x3F /* Flag Register */

/* XADC Configuration Registers */
#define ZYNQ7000_XADC_CFR0_OFFSET                   0x40 /* Configuration Register 0 */
#define ZYNQ7000_XADC_CFR1_OFFSET                   0x41 /* Configuration Register 1 */
#define ZYNQ7000_XADC_CFR2_OFFSET                   0x42 /* Configuration Register 2 */

/* XADC Sequence Registers */
#define ZYNQ7000_XADC_SEQ00_OFFSET                  0x48 /* Seq Reg 00 Adc Channel Selection */
#define ZYNQ7000_XADC_SEQ01_OFFSET                  0x49 /* Seq Reg 01 Adc Channel Selection */
#define ZYNQ7000_XADC_SEQ02_OFFSET                  0x4A /* Seq Reg 02 Adc Average Enable */
#define ZYNQ7000_XADC_SEQ03_OFFSET                  0x4B /* Seq Reg 03 Adc Average Enable */
#define ZYNQ7000_XADC_SEQ04_OFFSET                  0x4C /* Seq Reg 04 Adc Input Mode Select */
#define ZYNQ7000_XADC_SEQ05_OFFSET                  0x4D /* Seq Reg 05 Adc Input Mode Select */
#define ZYNQ7000_XADC_SEQ06_OFFSET                  0x4E /* Seq Reg 06 Adc Acquisition Select */
#define ZYNQ7000_XADC_SEQ07_OFFSET                  0x4F /* Seq Reg 07 Adc Acquisition Select */

/* XADC Alarm Threshold/Limit Registers (ATR) */
#define ZYNQ7000_XADC_ATR_TEMP_UPPER_OFFSET         0x50 /* Temp Upper Alarm Register */
#define ZYNQ7000_XADC_ATR_VCCINT_UPPER_OFFSET       0x51 /* VCCINT Upper Alarm Reg */
#define ZYNQ7000_XADC_ATR_VCCAUX_UPPER_OFFSET       0x52 /* VCCAUX Upper Alarm Reg */
#define ZYNQ7000_XADC_ATR_OT_UPPER_OFFSET           0x53 /* Over Temp Upper Alarm Reg */
#define ZYNQ7000_XADC_ATR_TEMP_LOWER_OFFSET         0x54 /* Temp Lower Alarm Register */
#define ZYNQ7000_XADC_ATR_VCCINT_LOWER_OFFSET       0x55 /* VCCINT Lower Alarm Reg */
#define ZYNQ7000_XADC_ATR_VCCAUX_LOWER_OFFSET       0x56 /* VCCAUX Lower Alarm Reg */
#define ZYNQ7000_XADC_ATR_OT_LOWER_OFFSET           0x57 /* Over Temp Lower Alarm Reg */
#define ZYNQ7000_XADC_ATR_VBRAM_UPPER_OFFSET        0x58 /* VBRAM Upper Alarm, 7 series */
#define ZYNQ7000_XADC_ATR_VCCPINT_UPPER_OFFSET      0x59 /* VCCPINT Upper Alarm, Zynq */
#define ZYNQ7000_XADC_ATR_VCCPAUX_UPPER_OFFSET      0x5A /* VCCPAUX Upper Alarm, Zynq */
#define ZYNQ7000_XADC_ATR_VCCPDRO_UPPER_OFFSET      0x5B /* VCCPDRO Upper Alarm, Zynq */
#define ZYNQ7000_XADC_ATR_VBRAM_LOWER_OFFSET        0x5C /* VRBAM Lower Alarm, 7 Series */
#define ZYNQ7000_XADC_ATR_VCCPINT_LOWER_OFFSET      0x5D /* VCCPINT Lower Alarm, Zynq */
#define ZYNQ7000_XADC_ATR_VCCPAUX_LOWER_OFFSET      0x5E /* VCCPAUX Lower Alarm, Zynq */
#define ZYNQ7000_XADC_ATR_VCCPDRO_LOWER_OFFSET      0x5F /* VCCPDRO Lower Alarm, Zynq */

/* Configuration Register 0 (CFR0) mask(s) */
#define ZYNQ7000_XADC_CFR0_CAL_AVG_MASK             0x8000 /* Averaging enable Mask */
#define ZYNQ7000_XADC_CFR0_AVG_VALID_MASK           0x3000 /* Averaging bit Mask */
#define ZYNQ7000_XADC_CFR0_AVG1_MASK                0x0000 /* No Averaging */
#define ZYNQ7000_XADC_CFR0_AVG16_MASK               0x1000 /* Average 16 samples */
#define ZYNQ7000_XADC_CFR0_AVG64_MASK               0x2000 /* Average 64 samples */
#define ZYNQ7000_XADC_CFR0_AVG256_MASK              0x3000 /* Average 256 samples */
#define ZYNQ7000_XADC_CFR0_AVG_SHIFT                12     /* Averaging bits shift */
#define ZYNQ7000_XADC_CFR0_MUX_MASK                 0x0800 /* External Mask Enable */
#define ZYNQ7000_XADC_CFR0_DU_MASK                  0x0400 /* Bipolar/Unipolar mode */
#define ZYNQ7000_XADC_CFR0_EC_MASK                  0x0200 /* Event driven continuous mode selection */
#define ZYNQ7000_XADC_CFR0_ACQ_MASK                 0x0100 /* Add acquisition by 6 ADCCLK */
#define ZYNQ7000_XADC_CFR0_CHANNEL_MASK             0x001F /* Channel number bit Mask */

/* Configuration Register 1 (CFR1) mask(s) */
#define ZYNQ7000_XADC_CFR1_SEQ_VALID_MASK           0xF000 /* Sequence bit Mask */
#define ZYNQ7000_XADC_CFR1_SEQ_SAFEMODE_MASK        0x0000 /* Default Safe Mode */
#define ZYNQ7000_XADC_CFR1_SEQ_ONEPASS_MASK         0x1000 /* Onepass through Seq */
#define ZYNQ7000_XADC_CFR1_SEQ_CONTINPASS_MASK      0x2000 /* Continuous Cycling Seq */
#define ZYNQ7000_XADC_CFR1_SEQ_SINGCHAN_MASK        0x3000 /* Single channel - No Seq */
#define ZYNQ7000_XADC_CFR1_SEQ_SIMUL_SAMPLING_MASK  0x4000 /* Simulataneous Sampling Mask */
#define ZYNQ7000_XADC_CFR1_SEQ_INDEPENDENT_MASK     0x8000 /* Independent Mode */
#define ZYNQ7000_XADC_CFR1_SEQ_SHIFT                12     /* Sequence bit shift */
#define ZYNQ7000_XADC_CFR1_ALM_VCCPDRO_MASK         0x0800 /* Alm 6 - VCCPDRO, Zynq  */
#define ZYNQ7000_XADC_CFR1_ALM_VCCPAUX_MASK         0x0400 /* Alm 5 - VCCPAUX, Zynq */
#define ZYNQ7000_XADC_CFR1_ALM_VCCPINT_MASK         0x0200 /* Alm 4 - VCCPINT, Zynq */
#define ZYNQ7000_XADC_CFR1_ALM_VBRAM_MASK           0x0100 /* Alm 3 - VBRAM, 7 series */
#define ZYNQ7000_XADC_CFR1_CAL_VALID_MASK           0x00F0 /* Valid Calibration Mask */
#define ZYNQ7000_XADC_CFR1_CAL_PS_GAIN_OFFSET_MASK  0x0080 /* Calibration 3 -Power Supply Gain/Offset Enable */
#define ZYNQ7000_XADC_CFR1_CAL_PS_OFFSET_MASK       0x0040 /* Calibration 2 -Power Supply Offset Enable */
#define ZYNQ7000_XADC_CFR1_CAL_ADC_GAIN_OFFSET_MASK 0x0020 /* Calibration 1 -ADC Gain Offset Enable */
#define ZYNQ7000_XADC_CFR1_CAL_ADC_OFFSET_MASK      0x0010 /* Calibration 0 -ADC Offset Enable */
#define ZYNQ7000_XADC_CFR1_CAL_DISABLE_MASK         0x0000 /* No Calibration */
#define ZYNQ7000_XADC_CFR1_ALM_ALL_MASK             0x0F0F /* Mask for all alarms */
#define ZYNQ7000_XADC_CFR1_ALM_VCCAUX_MASK          0x0008 /* Alarm 2 - VCCAUX Enable */
#define ZYNQ7000_XADC_CFR1_ALM_VCCINT_MASK          0x0004 /* Alarm 1 - VCCINT Enable */
#define ZYNQ7000_XADC_CFR1_ALM_TEMP_MASK            0x0002 /* Alarm 0 - Temperature */
#define ZYNQ7000_XADC_CFR1_OT_MASK                  0x0001 /* Over Temperature Enable */

/* Configuration Register 2 (CFR2) mask(s) */
#define ZYNQ7000_XADC_CFR2_CD_VALID_MASK            0xFF00 /* Clock Divisor bit Mask   */
#define ZYNQ7000_XADC_CFR2_CD_SHIFT                 8      /* Num of shift on division */
#define ZYNQ7000_XADC_CFR2_CD_MIN                   8      /* Minimum value of divisor */
#define ZYNQ7000_XADC_CFR2_CD_MAX                   255    /* Maximum value of divisor */

#define ZYNQ7000_XADC_CFR2_CD_MIN                   8      /* Minimum value of divisor */
#define ZYNQ7000_XADC_CFR2_PD_MASK                  0x0030 /* Power Down Mask */
#define ZYNQ7000_XADC_CFR2_PD_XADC_MASK             0x0030 /* Power Down XADC Mask */
#define ZYNQ7000_XADC_CFR2_PD_ADC1_MASK             0x0020 /* Power Down ADC1 Mask */
#define ZYNQ7000_XADC_CFR2_PD_SHIFT                 4      /* Power Down Shift */

/* Sequence Register (SEQ) Bit Definitions */
#define ZYNQ7000_XADC_SEQ_CH_CALIB                  0x00000001 /* ADC Calibration Channel */
#define ZYNQ7000_XADC_SEQ_CH_VCCPINT                0x00000020 /* VCCPINT, Zynq Only */
#define ZYNQ7000_XADC_SEQ_CH_VCCPAUX                0x00000040 /* VCCPAUX, Zynq Only */
#define ZYNQ7000_XADC_SEQ_CH_VCCPDRO                0x00000080 /* VCCPDRO, Zynq Only */
#define ZYNQ7000_XADC_SEQ_CH_TEMP                   0x00000100 /* On Chip Temperature Channel */
#define ZYNQ7000_XADC_SEQ_CH_VCCINT                 0x00000200 /* VCCINT Channel */
#define ZYNQ7000_XADC_SEQ_CH_VCCAUX                 0x00000400 /* VCCAUX Channel */
#define ZYNQ7000_XADC_SEQ_CH_VPVN                   0x00000800 /* VP/VN analog inputs Channel */
#define ZYNQ7000_XADC_SEQ_CH_VREFP                  0x00001000 /* VREFP Channel */
#define ZYNQ7000_XADC_SEQ_CH_VREFN                  0x00002000 /* VREFN Channel */
#define ZYNQ7000_XADC_SEQ_CH_VBRAM                  0x00004000 /* VBRAM Channel, 7 series */
#define ZYNQ7000_XADC_SEQ_CH_AUX00                  0x00010000 /* 1st Aux Channel */
#define ZYNQ7000_XADC_SEQ_CH_AUX01                  0x00020000 /* 2nd Aux Channel */
#define ZYNQ7000_XADC_SEQ_CH_AUX02                  0x00040000 /* 3rd Aux Channel */
#define ZYNQ7000_XADC_SEQ_CH_AUX03                  0x00080000 /* 4th Aux Channel */
#define ZYNQ7000_XADC_SEQ_CH_AUX04                  0x00100000 /* 5th Aux Channel */
#define ZYNQ7000_XADC_SEQ_CH_AUX05                  0x00200000 /* 6th Aux Channel */
#define ZYNQ7000_XADC_SEQ_CH_AUX06                  0x00400000 /* 7th Aux Channel */
#define ZYNQ7000_XADC_SEQ_CH_AUX07                  0x00800000 /* 8th Aux Channel */
#define ZYNQ7000_XADC_SEQ_CH_AUX08                  0x01000000 /* 9th Aux Channel */
#define ZYNQ7000_XADC_SEQ_CH_AUX09                  0x02000000 /* 10th Aux Channel */
#define ZYNQ7000_XADC_SEQ_CH_AUX10                  0x04000000 /* 11th Aux Channel */
#define ZYNQ7000_XADC_SEQ_CH_AUX11                  0x08000000 /* 12th Aux Channel */
#define ZYNQ7000_XADC_SEQ_CH_AUX12                  0x10000000 /* 13th Aux Channel */
#define ZYNQ7000_XADC_SEQ_CH_AUX13                  0x20000000 /* 14th Aux Channel */
#define ZYNQ7000_XADC_SEQ_CH_AUX14                  0x40000000 /* 15th Aux Channel */
#define ZYNQ7000_XADC_SEQ_CH_AUX15                  0x80000000 /* 16th Aux Channel */

#define ZYNQ7000_XADC_SEQ00_CH_VALID_MASK           0x7FE1 /* Mask for the valid channels */
#define ZYNQ7000_XADC_SEQ01_CH_VALID_MASK           0xFFFF /* Mask for the valid channels */
#define ZYNQ7000_XADC_SEQ02_CH_VALID_MASK           0x7FE0 /* Mask for the valid channels */
#define ZYNQ7000_XADC_SEQ03_CH_VALID_MASK           0xFFFF /* Mask for the valid channels */
#define ZYNQ7000_XADC_SEQ04_CH_VALID_MASK           0x0800 /* Mask for the valid channels */
#define ZYNQ7000_XADC_SEQ05_CH_VALID_MASK           0xFFFF /* Mask for the valid channels */
#define ZYNQ7000_XADC_SEQ06_CH_VALID_MASK           0x0800 /* Mask for the valid channels */
#define ZYNQ7000_XADC_SEQ07_CH_VALID_MASK           0xFFFF /* Mask for the valid channels */

#define ZYNQ7000_XADC_SEQ_CH_AUX_SHIFT              16 /* Shift for the Aux Channel */

/* OT Upper Alarm Threshold Register Bit Definitions */

#define ZYNQ7000_XADC_ATR_OT_UPPER_ENB_MASK         0x000F /* Mask for OT enable */
#define ZYNQ7000_XADC_ATR_OT_UPPER_VAL_MASK         0xFFF0 /* Mask for OT value */
#define ZYNQ7000_XADC_ATR_OT_UPPER_VAL_SHIFT        4      /* Shift for OT value */
#define ZYNQ7000_XADC_ATR_OT_UPPER_ENB_VAL          0x0003 /* Value for OT enable */
#define ZYNQ7000_XADC_ATR_OT_UPPER_VAL_MAX          0x0FFF /* Max OT value */

/* JTAG DRP Bit Definitions */
#define ZYNQ7000_XADC_JTAG_DATA_MASK                0x0000FFFF /* Mask for the Data */
#define ZYNQ7000_XADC_JTAG_ADDR_MASK                0x03FF0000 /* Mask for the Addr */
#define ZYNQ7000_XADC_JTAG_ADDR_SHIFT               16         /* Shift for the Addr */
#define ZYNQ7000_XADC_JTAG_CMD_MASK                 0x3C000000 /* Mask for the Cmd */
#define ZYNQ7000_XADC_JTAG_CMD_WRITE_MASK           0x08000000 /* Mask for CMD Write */
#define ZYNQ7000_XADC_JTAG_CMD_READ_MASK            0x04000000 /* Mask for CMD Read */
#define ZYNQ7000_XADC_JTAG_CMD_SHIFT                26         /* Shift for the Cmd */

/* Indexes for the different channels. */
#define ZYNQ7000_XADC_CH_TEMP                       0x0  /* On Chip Temperature */
#define ZYNQ7000_XADC_CH_VCCINT                     0x1  /* VCCINT */
#define ZYNQ7000_XADC_CH_VCCAUX                     0x2  /* VCCAUX */
#define ZYNQ7000_XADC_CH_VPVN                       0x3  /* VP/VN Dedicated analog inputs */
#define ZYNQ7000_XADC_CH_VREFP                      0x4  /* VREFP */
#define ZYNQ7000_XADC_CH_VREFN                      0x5  /* VREFN */
#define ZYNQ7000_XADC_CH_VBRAM                      0x6  /* On-chip VBRAM Data Reg, 7 series */
#define ZYNQ7000_XADC_CH_SUPPLY_CALIB               0x07 /* Supply Calib Data Reg */
#define ZYNQ7000_XADC_CH_ADC_CALIB                  0x08 /* ADC Offset Channel Reg */
#define ZYNQ7000_XADC_CH_GAINERR_CALIB              0x09 /* Gain Error Channel Reg  */
#define ZYNQ7000_XADC_CH_VCCPINT                    0x0D /* On-chip PS VCCPINT Channel */
#define ZYNQ7000_XADC_CH_VCCPAUX                    0x0E /* On-chip PS VCCPAUX Channel */
#define ZYNQ7000_XADC_CH_VCCPDRO                    0x0F /* On-chip PS VCCPDRO Channel */
#define ZYNQ7000_XADC_CH_AUX_MIN                    16   /* Channel number for 1st Aux Channel */
#define ZYNQ7000_XADC_CH_AUX_MAX                    31   /* Channel number for Last Aux channel */

/* Alarm Threshold(Limit) Register (ATR) indexes. */
#define XADC_ATR_TEMP_UPPER                         0   /* High user Temperature */
#define XADC_ATR_VCCINT_UPPER                       1   /* VCCINT high voltage limit register */
#define XADC_ATR_VCCAUX_UPPER                       2   /* VCCAUX high voltage limit register */
#define XADC_ATR_OT_UPPER                           3   /* VCCAUX high voltage limit register */
#define XADC_ATR_TEMP_LOWER                         4   /* Upper Over Temperature limit Reg */
#define XADC_ATR_VCCINT_LOWER                       5   /* VCCINT high voltage limit register */
#define XADC_ATR_VCCAUX_LOWER                       6   /* VCCAUX low voltage limit register  */
#define XADC_ATR_OT_LOWER                           7   /* Lower Over Temperature limit */
#define XADC_ATR_VBRAM_UPPER_                       8   /* VRBAM Upper Alarm Reg, 7 Series */
#define XADC_ATR_VCCPINT_UPPER                      9   /* VCCPINT Upper Alarm Reg, Zynq */
#define XADC_ATR_VCCPAUX_UPPER                      0xA /* VCCPAUX Upper Alarm Reg, Zynq */
#define XADC_ATR_VCCPDRO_UPPER                      0xB /* VCCPDRO Upper Alarm Reg, Zynq */
#define XADC_ATR_VBRAM_LOWER                        0xC /* VRBAM Lower Alarm Reg, 7 Series */
#define XADC_ATR_VCCPINT_LOWER                      0xD /* VCCPINT Lower Alarm Reg , Zynq */
#define XADC_ATR_VCCPAUX_LOWER                      0xE /* VCCPAUX Lower Alarm Reg , Zynq */
#define XADC_ATR_VCCPDRO_LOWER                      0xF /* VCCPDRO Lower Alarm Reg , Zynq */

/* Indexes for reading the Minimum/Maximum Measurement Data. */
#define ZYNQ7000_XADC_MAX_TEMP                      0   /* Maximum Temperature Data */
#define ZYNQ7000_XADC_MAX_VCCINT                    1   /* Maximum VCCINT Data */
#define ZYNQ7000_XADC_MAX_VCCAUX                    2   /* Maximum VCCAUX Data */
#define ZYNQ7000_XADC_MAX_VBRAM                     3   /* Maximum VBRAM Data */
#define ZYNQ7000_XADC_MIN_TEMP                      4   /* Minimum Temperature Data */
#define ZYNQ7000_XADC_MIN_VCCINT                    5   /* Minimum VCCINT Data */
#define ZYNQ7000_XADC_MIN_VCCAUX                    6   /* Minimum VCCAUX Data */
#define ZYNQ7000_XADC_MIN_VBRAM                     7   /* Minimum VBRAM Data */
#define ZYNQ7000_XADC_MAX_VCCPINT                   8   /* Maximum VCCPINT Register */
#define ZYNQ7000_XADC_MAX_VCCPAUX                   9   /* Maximum VCCPAUX Register */
#define ZYNQ7000_XADC_MAX_VCCPDRO                   0xA /* Maximum VCCPDRO Register */
#define ZYNQ7000_XADC_MIN_VCCPINT                   0xC /* Minimum VCCPINT Register */
#define ZYNQ7000_XADC_MIN_VCCPAUX                   0xD /* Minimum VCCPAUX Register */
#define ZYNQ7000_XADC_MIN_VCCPDRO                   0xE /* Minimum VCCPDRO Register */

/* Indexes for reading the Calibration Coefficient Data. */
#define ZYNQ7000_XADC_CALIB_SUPPLY_COEFF            0 /* Supply Offset Calib Coefficient */
#define ZYNQ7000_XADC_CALIB_ADC_COEFF               1 /* ADC Offset Calib Coefficient */
#define ZYNQ7000_XADC_CALIB_GAIN_ERROR_COEFF        2 /* Gain Error Calib Coefficient*/

/* Averaging to be done for the channels. */
#define ZYNQ7000_XADC_AVG_0_SAMPLES                 0 /* No Averaging */
#define ZYNQ7000_XADC_AVG_16_SAMPLES                1 /* Average 16 samples */
#define ZYNQ7000_XADC_AVG_64_SAMPLES                2 /* Average 64 samples */
#define ZYNQ7000_XADC_AVG_256_SAMPLES               3 /* Average 256 samples */

/* Channel Sequencer Modes of operation */
#define ZYNQ7000_XADC_SEQ_MODE_SAFE                 0 /* Default Safe Mode */
#define ZYNQ7000_XADC_SEQ_MODE_ONEPASS              1 /* Onepass through Sequencer */
#define ZYNQ7000_XADC_SEQ_MODE_CONTINPASS           2 /* Continuous Cycling Sequencer */
#define ZYNQ7000_XADC_SEQ_MODE_SINGCHAN             3 /* Single channel -No Sequencing */
#define ZYNQ7000_XADC_SEQ_MODE_SIMUL_SAMPLING       4 /* Simultaneous sampling */
#define ZYNQ7000_XADC_SEQ_MODE_INDEPENDENT          8 /* Independent mode */

/* Power Down Modes */
#define ZYNQ7000_XADC_PD_MODE_NONE                  0 /* No Power Down  */
#define ZYNQ7000_XADC_PD_MODE_ADCB                  1 /* Power Down ADC B */
#define ZYNQ7000_XADC_PD_MODE_ADCAB                 2 /* Power Down ADC A and ADC B */

#define ZYNQ7000_XADC_NUM_ALARMS                    8 /* The number of alarms that are available to be enabled */
#define ZYNQ7000_XADC_ALARM_ENABLE                  1 /* Use to set the enable bit in an alarm_enables_write_t */
#define ZYNQ7000_XADC_ALARM_DISABLE                 0

/*
 * Resource Manager Interface
 */

#define ZYNQ7000_XADC_NAME "/dev/xadc"

typedef struct {
	uint8_t channel;
	uint16_t data;
} zynq7000_xadc_data_read_t;

typedef struct {
	uint8_t coeff_type;
	uint16_t calib_coeff;
} zynq7000_xadc_calib_coeff_read_t;

typedef struct {
	uint8_t measurement_type;
	uint16_t min_max_measure;
} zynq7000_xadc_min_max_measure_read_t;

typedef struct {
	uint8_t channel;
	int increase_acq_cycles;
	int is_event_mode;
	int is_differential_mode;
	int result_code;
} zynq7000_xadc_single_ch_params_t;

typedef struct {
	uint16_t mask;
	struct sigevent event;
	int enable;
} zynq7000_xadc_alarm_enables_write_t;

typedef struct {
	uint32_t mask;
	int result_code;
} zynq7000_xadc_seq_write_t;

typedef struct {
	uint8_t alarm_thr_reg;
	uint16_t value;
} zynq7000_xadc_alarm_threshold_t;

typedef struct {
	int mux_mode;
	uint8_t channel;
} zynq7000_xadc_mux_mode_t;

typedef struct {
	uint32_t offset;
	uint32_t data;
} zynq7000_xadc_internal_reg_t;

/*
 * The following devctls are used by a client application
 * to control the XADC interface.
 */

#include <devctl.h>

#define _DCMD_XADC                       _DCMD_MISC

#define DCMD_XADC_CFG_INIT               __DION(_DCMD_XADC, 1)
#define DCMD_XADC_SET_CFG_REG            __DIOT(_DCMD_XADC, 2, uint32_t)
#define DCMD_XADC_GET_CFG_REG            __DIOF(_DCMD_XADC, 3, uint32_t)
#define DCMD_XADC_GET_MISC_STATUS        __DIOF(_DCMD_XADC, 4, uint32_t)
#define DCMD_XADC_GET_MISC_CTRL_REG      __DIOF(_DCMD_XADC, 5, uint32_t)
#define DCMD_XADC_RESET                  __DION(_DCMD_XADC, 6)
#define DCMD_XADC_GET_ADC_DATA           __DIOTF(_DCMD_XADC, 7, zynq7000_xadc_data_read_t)
#define DCMD_XADC_GET_CALIB_COEFF        __DIOTF(_DCMD_XADC, 8, zynq7000_xadc_calib_coeff_read_t)
#define DCMD_XADC_GET_MIN_MAX_MSRMT      __DIOTF(_DCMD_XADC, 9, zynq7000_xadc_min_max_measure_read_t)
#define DCMD_XADC_SET_AVG                __DIOT(_DCMD_XADC, 10, uint8_t)
#define DCMD_XADC_GET_AVG                __DIOF(_DCMD_XADC, 11, uint8_t)
#define DCMD_XADC_SET_SINGLE_CH_PARAM    __DIOTF(_DCMD_XADC, 12, zynq7000_xadc_single_ch_params_t)
#define DCMD_XADC_SET_ALARM_ENABLES      __DIOT(_DCMD_XADC, 13, zynq7000_xadc_alarm_enables_write_t)
#define DCMD_XADC_GET_ALARM_ENABLES      __DIOF(_DCMD_XADC, 14, uint16_t)
#define DCMD_XADC_SET_CALIB_ENABLES      __DIOT(_DCMD_XADC, 15, uint16_t)
#define DCMD_XADC_GET_CALIB_ENABLES      __DIOF(_DCMD_XADC, 16, uint16_t)
#define DCMD_XADC_SET_SEQUENCER_MODE     __DIOT(_DCMD_XADC, 17, uint8_t)
#define DCMD_XADC_GET_SEQUENCER_MODE     __DIOF(_DCMD_XADC, 18, uint8_t)
#define DCMD_XADC_SET_ADC_CLK_DIVISOR    __DIOT(_DCMD_XADC, 19, uint8_t)
#define DCMD_XADC_GET_ADC_CLK_DIVISOR    __DIOF(_DCMD_XADC, 20, uint8_t)
#define DCMD_XADC_SET_SEQ_CH_ENABLES     __DIOTF(_DCMD_XADC, 21, zynq7000_xadc_seq_write_t)
#define DCMD_XADC_GET_SEQ_CH_ENABLES     __DIOF(_DCMD_XADC, 22, uint32_t)
#define DCMD_XADC_SET_SEQ_AVG_ENABLES    __DIOTF(_DCMD_XADC, 23, zynq7000_xadc_seq_write_t)
#define DCMD_XADC_GET_SEQ_AVG_ENABLES    __DIOF(_DCMD_XADC, 24, uint32_t)
#define DCMD_XADC_SET_SEQ_INPUT_MODE     __DIOTF(_DCMD_XADC, 25, zynq7000_xadc_seq_write_t)
#define DCMD_XADC_GET_SEQ_INPUT_MODE     __DIOF(_DCMD_XADC, 26, uint32_t)
#define DCMD_XADC_SET_SEQ_ACQ_TIME       __DIOTF(_DCMD_XADC, 27, zynq7000_xadc_seq_write_t)
#define DCMD_XADC_GET_SEQ_ACQ_TIME       __DIOF(_DCMD_XADC, 28, uint32_t)
#define DCMD_XADC_SET_ALARM_THRESHOLD    __DIOT(_DCMD_XADC, 29, zynq7000_xadc_alarm_threshold_t)
#define DCMD_XADC_GET_ALARM_THRESHOLD    __DIOTF(_DCMD_XADC, 30, zynq7000_xadc_alarm_threshold_t)
#define DCMD_XADC_ENABLE_USER_OVER_TEMP  __DION(_DCMD_XADC, 31)
#define DCMD_XADC_DISABLE_USER_OVER_TEMP __DION(_DCMD_XADC, 32)
#define DCMD_XADC_SET_SEQUENCER_EVENT    __DIOT(_DCMD_XADC, 33, int)
#define DCMD_XADC_GET_SAMPLING_MODE      __DIOF(_DCMD_XADC, 34, int)
#define DCMD_XADC_SET_MUX_MODE           __DIOT(_DCMD_XADC, 35, zynq7000_xadc_mux_mode_t)
#define DCMD_XADC_SET_POWER_DOWN_MODE    __DIOT(_DCMD_XADC, 36, uint32_t)
#define DCMD_XADC_GET_POWER_DOWN_MODE    __DIOF(_DCMD_XADC, 37, uint32_t)
#define DCMD_XADC_WRITE_INTERNAL_REG     __DIOT(_DCMD_XADC, 38, zynq7000_xadc_internal_reg_t)
#define DCMD_XADC_READ_INTERNAL_REG      __DIOTF(_DCMD_XADC, 39, zynq7000_xadc_internal_reg_t)
#define DCMD_XADC_INTR_ENABLE            __DIOT(_DCMD_XADC, 40, uint32_t)
#define DCMD_XADC_INTR_DISABLE           __DIOT(_DCMD_XADC, 41, uint32_t)
#define DCMD_XADC_INTR_GET_ENABLED       __DIOF(_DCMD_XADC, 42, uint32_t)
#define DCMD_XADC_INTR_GET_STATUS        __DIOF(_DCMD_XADC, 43, uint32_t)
#define DCMD_XADC_INTR_CLEAR             __DIOT(_DCMD_XADC, 44, uint32_t)

#endif /* __HW_XADC_H__ */
