/*
 * $QNXLicenseC:
 * Copyright 2016, QNX Software Systems.
 * Copyright 2016, Freescale Semiconductor, Inc.
 * Copyright 2017 NXP
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You
 * may not reproduce, modify or distribute this software except in
 * compliance with the License. You may obtain a copy of the License
 * at: http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 *
 * This file may contain contributions from others, either as
 * contributors under the License or as licensors under other terms.
 * Please review this entire file for other proprietary rights or license
 * notices, as well as the QNX Development Suite License Guide at
 * http://licensing.qnx.com/license-guide/ for other information.
 * $
 */

#ifndef IMX_QSPI_H_
#define IMX_QSPI_H_

/* Registers offset */
#define IMX_QSPI_MCR_OFFSET             0x00   /**< Module Configuration Register */
#define IMX_QSPI_IPCR_OFFSET            0x08   /**< IP Configuration Register */
#define IMX_QSPI_FLSHCR_OFFSET          0x0c   /**< Flash Configuration Register */
#define IMX_QSPI_BUF0CR_OFFSET          0x10   /**< Buffer0 Configuration Register */
#define IMX_QSPI_BUF1CR_OFFSET          0x14   /**< Buffer1 Configuration Register */
#define IMX_QSPI_BUF2CR_OFFSET          0x18   /**< Buffer2 Configuration Register */
#define IMX_QSPI_BUF3CR_OFFSET          0x1C   /**< Buffer3 Configuration Register */
#define IMX_QSPI_BFGENCR_OFFSET         0x20   /**< Buffer Generic Configuration Register */
#define IMX_QSPI_BUF0IND_OFFSET         0x30   /**< Buffer0 Top Index Register */
#define IMX_QSPI_BUF1IND_OFFSET         0x34   /**< Buffer1 Top Index Register */
#define IMX_QSPI_BUF2IND_OFFSET         0x38   /**< Buffer2 Top Index Register */
#define IMX_QSPI_SFAR_OFFSET            0x100  /**< Serial Flash Address Register */
#define IMX_QSPI_SMPR_OFFSET            0x108  /**< Sampling Register */
#define IMX_QSPI_RBSR_OFFSET            0x10C  /**< RX Buffer Status Register */
#define IMX_QSPI_RBCT_OFFSET            0x110  /**< RX Buffer Control Register */
#define IMX_QSPI_TBSR_OFFSET            0x150  /**< TX Buffer Status Register */
#define IMX_QSPI_TBDR_OFFSET            0x154  /**< TX Buffer Data Register */
#define IMX_QSPI_SR_OFFSET              0x15C  /**< Status Register */
#define IMX_QSPI_FR_OFFSET              0x160  /**< Flag Register */
#define IMX_QSPI_RSER_OFFSET            0x164  /**< Interrupt and DMA Request Select and Enable Register */
#define IMX_QSPI_SPNDST_OFFSET          0x168  /**< Sequence Suspend Status Register */
#define IMX_QSPI_SPTRCLR_OFFSET         0x16C  /**< Sequence Pointer Clear Register */
#define IMX_QSPI_SFA1AD_OFFSET          0x180  /**< Serial Flash A1Top Address */
#define IMX_QSPI_SFA2AD_OFFSET          0x184  /**< Serial Flash A2Top Address */
#define IMX_QSPI_SFB1AD_OFFSET          0x188  /**< Serial Flash B1Top Address */
#define IMX_QSPI_SFB2AD_OFFSET          0x18C  /**< Serial Flash B2Top Address */
#define IMX_QSPI_RBDR0_OFFSET           0x200  /**< RX Buffer Data Register 0 */
#define IMX_QSPI_RBDRn_OFFSET(index)    (IMX_QSPI_RBDR0_OFFSET + (index * 4)) /**< RX Buffer Data Register n */
#define IMX_QSPI_LUTKEY_OFFSET          0x300  /**< LUT Key Register */
#define IMX_QSPI_LCKCR_OFFSET           0x304  /**< LUT Lock Configuration Register */
#define IMX_QSPI_LUT0_OFFSET            0x310  /**< Look-up Table register 0 */
#define IMX_QSPI_LUT1_OFFSET            0x314  /**< Look-up Table register 1 */
#define IMX_QSPI_LUT2_OFFSET            0x318  /**< Look-up Table register 2 */
#define IMX_QSPI_LUT3_OFFSET            0x31C  /**< Look-up Table register 3 */
#define IMX_QSPI_LUTn_OFFSET(index)     (IMX_QSPI_LUT0_OFFSET + (index * 4)) /**< Look-up Table register n */

/* Registers mask */

/* Module Configuration Register */
#define IMX_QSPI_MCR_MDIS_MASK          0x4000 /**< Module Disable */
#define IMX_QSPI_MCR_CLR_TXF_MASK       0x800  /**< Clear TX FIFO/Buffer */
#define IMX_QSPI_MCR_CLR_RXF_MASK       0x400  /**< Clear RX FIFO/Buffer */
#define IMX_QSPI_MCR_DDR_EN             0x80   /**< SDR and DDR instructions are supported */
#define IMX_QSPI_MCR_SWRSTHD_MASK       0x2    /**< Software reset for QSPI AHB domain */
#define IMX_QSPI_MCR_SWRSTSD_MASK       0x1    /**< Software reset for Serial Flash domain */

/* IP Configuration Register */
#define IMX_QSPI_IPCR_SEQID_MASK        0x7800000 /**< Points to a sequence in the Look-up-table. A write triggers a transaction. */
#define IMX_QSPI_IPCR_SEQID_SHIFT       24

/* Flash Configuration Register */
#define IMX_QSPI_TCSS_MASK              0xF    /**< Serial flash CS setup time */
#define IMX_QSPI_TCSH_MASK              0xF00  /**< Serial flash CS hold time */

/* RX Buffer Status Register */
#define IMX_QSPI_RBSR_RDBFL_MASK        0x3F00 /**< RX Buffer Fill Level (4 bytes) entries count */
#define IMX_QSPI_RBSR_RDBFL_SHIFT       8

/* Status Register */
#define IMX_QSPI_SR_TXFULL_MASK         0x8000000       /**< TX Buffer Full. Asserted when no more data can be stored. */
#define IMX_QSPI_SR_IP_ACC_MASK         0x2             /**< IP Access: Asserted when transaction currently executed was initiated by IP bus. */
#define IMX_QSPI_SR_BUSY_MASK           0x1             /**< Module Busy: Module is handling a transaction to an external flash device. */

/* RX Buffer Control Register */
#define IMX_QSPI_RBCT_RXBRD_MASK        0x100   /**< RX Buffer Readout: 1: RBDR0 to RBDRn */

/* Sequence Pointer Clear Register */
#define IMX_QSPI_SPTRCLR_IPPTRC_MASK    0x100   /**< 1: Clears the sequence pointer for IP accesses as defined in IPCR. */
#define IMX_QSPI_SPTRCLR_BFPTRC_MASK    0x1     /**< 1: Clears the sequence pointer for AHB accesses as defined in BFGENCR. */

/* RSER */
/* RX Buffer Drain Interrupt Enable */
#define IMX_QSPI_RSER_RBDIE_SHIFT       16
#define IMX_QSPI_RSER_RBDIE_MASK        (1 << IMX_QSPI_RSER_RBDIE_SHIFT)

/* TX Buffer Fill Interrupt Enable */
#define IMX_QSPI_RSER_TBFIE_SHIFT       27
#define IMX_QSPI_RSER_TBFIE_MASK        (1 << IMX_QSPI_RSER_TBFIE_SHIFT)

/* Transaction Finished Interrupt Enable */
#define IMX_QSPI_RSER_TFIE_SHIFT        1
#define IMX_QSPI_RSER_TFIE_MASK         (1 << IMX_QSPI_RSER_TFIE_SHIFT)

#endif /* IMX_QSPI_H_ */
