/*
 *
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 *
 */

#include <sys/platform.h>

#ifndef __RK3568_SPI_H__
#define __RK3568_SPI_H__

#define RK3568_SPI0_BASE						0xfe610000
#define RK3568_SPI1_BASE						0xfe620000
#define RK3568_SPI2_BASE						0xfe630000
#define RK3568_SPI3_BASE						0xfe640000

#define RK3568_SPI0_IRQ							135
#define RK3568_SPI1_IRQ							136
#define RK3568_SPI2_IRQ							137
#define RK3568_SPI3_IRQ							138

/* SPI register offsets */
#define RK3568_SPI_CTRLR0						0x0000		/* Control Register 0 */
#define RK3568_SPI_CTRLR1						0x0004		/* Control Register 1 */
#define RK3568_SPI_ENR							0x0008		/* SPI Enable Register */
#define RK3568_SPI_SER							0x000c		/* Slave Enable Register */
#define RK3568_SPI_BAUDR						0x0010		/* Baud Rate */
#define RK3568_SPI_TXFTLR						0x0014		/* Transmit FIFO Threshold Level */
#define RK3568_SPI_RXFTLR						0x0018		/* Receive FIFO Threshold Level */
#define RK3568_SPI_TXFLR						0x001c		/* Transmit FIFO Level */
#define RK3568_SPI_RXFLR						0x0020		/* Receive FIFO Level */
#define RK3568_SPI_SR							0x0024		/* SPI Status */
#define RK3568_SPI_IPR							0x0028		/* Interrupt Polarity */
#define RK3568_SPI_IMR							0x002c		/* Interrupt Mask */
#define RK3568_SPI_ISR							0x0030		/* Interrupt Status */
#define RK3568_SPI_RISR							0x0034		/* Raw Interrupt Status (prior to masking) */
#define RK3568_SPI_ICR							0x0038		/* Interrupt Clear */
#define RK3568_SPI_DMACR						0x003c		/* DMA Control */
#define RK3568_SPI_DMATDLR						0x0040		/* DMA Transmit Data Level */
#define RK3568_SPI_DMARDLR						0x0044		/* DMA Receive Data Level */
#define RK3568_SPI_TIMEOUT						0x004c		/* Timeout control register */
#define RK3568_SPI_BYPASS						0x0050		/* BYPASS control register */
#define RK3568_SPI_TXDR							0x0400		/* Transmit FIFO Data */
#define RK3568_SPI_RXDR							0x0800		/* Receive FIFO Data */

/* reset value/mask */
#define RK3568_SPI_CTRLR0_RESET					0x2

/* Bit fields in SPI_CTRLR0 */
/* Data Frame Length */
#define RK3568_SPI_CTRLR0_DLEN_4  				0x0
#define RK3568_SPI_CTRLR0_DLEN_8  				0x1
#define RK3568_SPI_CTRLR0_DLEN_16				0x2

#define RK3568_SPI_CTRLR0_FRAME_FORMAT_MASK		0x0000003c	/* Length of the control word = 16 bit */
#define RK3568_SPI_CTRLR0_FRAME_FORMAT_OFFSET	0x2
#define RK3568_SPI_CTRLR0_FRAME_FORMAT_4		0x3
#define RK3568_SPI_CTRLR0_FRAME_FORMAT_8		0x7
#define RK3568_SPI_CTRLR0_FRAME_FORMAT_16		0xf

/* Clock Polarity/Phase Mode */
/* Valid when the frame format is set to Motorola SPI */
#define RK3568_SPI_CTRLR0_SCPHA_OFFSET 			6			/* 1'b1 : toggles at start. 1'b0 in middle */
#define RK3568_SPI_CTRLR0_SCPHA_MIDDLE			_BITFIELD32L(RK3568_SPI_CTRLR0_SCPHA_OFFSET, 0x0)
#define RK3568_SPI_CTRLR0_SCPHA_START			_BITFIELD32L(RK3568_SPI_CTRLR0_SCPHA_OFFSET, 0x1)

#define RK3568_SPI_CTRLR0_SCPOL_OFFSET			7			/* 1'b0 : inactive state clock is low, 1'b1 state is high */
#define RK3568_SPI_CTRLR0_SCPOL_LOW				_BITFIELD32L(RK3568_SPI_CTRLR0_SCPOL_OFFSET, 0x0)
#define RK3568_SPI_CTRLR0_SCPOL_HIGH			_BITFIELD32L(RK3568_SPI_CTRLR0_SCPOL_OFFSET, 0x1)

/* Period between cs active and sclk_out active */
#define RK3568_SPI_CTRLR0_SSD_OFFSET			10
#define RK3568_SPI_CTRLR0_SSD_HALF				_BITFIELD32L(RK3568_SPI_CTRLR0_SSD_OFFSET, 0x0)
#define RK3568_SPI_CTRLR0_SSD_ONE				_BITFIELD32L(RK3568_SPI_CTRLR0_SSD_OFFSET, 0x1)

/* Endian mode */
#define RK3568_SPI_CTRLR0_EM_OFFSET				11
#define RK3568_SPI_CTRLR0_EM_LE					_BITFIELD32L(RK3568_SPI_CTRLR0_EM_OFFSET, 0x0)
#define RK3568_SPI_CTRLR0_EM_BE					_BITFIELD32L(RK3568_SPI_CTRLR0_EM_OFFSET, 0x1)

/* Significant first bit */
#define RK3568_SPI_CTRLR0_FBM_OFFSET			12
#define RK3568_SPI_CTRLR0_FBM_MSB				_BITFIELD32L(RK3568_SPI_CTRLR0_FBM_OFFSET, 0x0)
#define RK3568_SPI_CTRLR0_FBM_LSB				_BITFIELD32L(RK3568_SPI_CTRLR0_FBM_OFFSET, 0x1)

#define RK3568_SPI_CTRLR0_BHT_OFFSET			13
#define RK3568_SPI_CTRLR0_BHT_16				_BITFIELD32L(RK3568_SPI_CTRLR0_BHT_OFFSET, 0x0)
#define RK3568_SPI_CTRLR0_BHT_8					_BITFIELD32L(RK3568_SPI_CTRLR0_BHT_OFFSET, 0x1)

#define RK3568_SPI_CTRLR0_RSD_OFFSET			14
#define RK3568_SPI_CTRLR0_RSD_ZERO_DELAY		_BITFIELD32L(RK3568_SPI_CTRLR0_RSD_OFFSET, 0x0)
#define RK3568_SPI_CTRLR0_RSD_ONE_CYCLE			_BITFIELD32L(RK3568_SPI_CTRLR0_RSD_OFFSET, 0x1)
#define RK3568_SPI_CTRLR0_RSD_TWO_CYCLES		_BITFIELD32L(RK3568_SPI_CTRLR0_RSD_OFFSET, 0x2)
#define RK3568_SPI_CTRLR0_RSD_THREE_CYCLES		_BITFIELD32L(RK3568_SPI_CTRLR0_RSD_OFFSET, 0x3)

/* Frame format */
#define RK3568_SPI_CTRLR0_FRF_OFFSET			16
#define RK3568_SPI_CTRLR0_FRF_MASK				_BITFIELD32L(RK3568_SPI_CTRLR0_FRF_OFFSET, 0x3)
#define RK3568_SPI_CTRLR0_FRF_SPI				_BITFIELD32L(RK3568_SPI_CTRLR0_FRF_OFFSET, 0x0)
#define RK3568_SPI_CTRLR0_FRF_SSP				_BITFIELD32L(RK3568_SPI_CTRLR0_FRF_OFFSET, 0x1)
#define RK3568_SPI_CTRLR0_FRF_MICROWIRE			_BITFIELD32L(RK3568_SPI_CTRLR0_FRF_OFFSET, 0x2)

/* Transmit/Receive Mode */
#define RK3568_SPI_CTRLR0_MODE_OFFSET			18
#define RK3568_SPI_CTRLR0_MODE_MASK				_BITFIELD32L(RK3568_SPI_CTRLR0_MODE_OFFSET, 0x3)
											/* Transmit & Receive */
#define RK3568_SPI_CTRLR0_MODE_TR				_BITFIELD32L(RK3568_SPI_CTRLR0_MODE_OFFSET, 0x0)
											/* Transmit Only */
#define RK3568_SPI_CTRLR0_MODE_TO				_BITFIELD32L(RK3568_SPI_CTRLR0_MODE_OFFSET, 0x1)
											/* Receive Only */
#define RK3568_SPI_CTRLR0_MODE_RO				_BITFIELD32L(RK3568_SPI_CTRLR0_MODE_OFFSET, 0x2)

/* Master and slave mode */
#define RK3568_SPI_CTRLR0_MSM_OFFSET			20
#define RK3568_SPI_CTRLR0_MSM_MASTER			_BITFIELD32L(RK3568_SPI_CTRLR0_MSM_OFFSET, 0x0)
#define RK3568_SPI_CTRLR0_MSM_SLAVE				_BITFIELD32L(RK3568_SPI_CTRLR0_MSM_OFFSET, 0x1)

/* Slave select inversion */
#define RK3568_SPI_CTRLR0_SOI_OFFSET			23
#define RK3568_SPI_CTRLR0_SOI_DEFAULT			_BITFIELD32L(RK3568_SPI_CTRLR0_SOI_OFFSET, 0x0)
#define RK3568_SPI_CTRLR0_SOI_INVERTED			_BITFIELD32L(RK3568_SPI_CTRLR0_SOI_OFFSET, 0x1)

/* Loop back mode select */
#define RK3568_SPI_CTRLR0_LBK_OFFSET			25
											/* Normal mode */
#define RK3568_SPI_CTRLR0_LBK_NM				_BITFIELD32L(RK3568_SPI_CTRLR0_LBK_OFFSET, 0x0)
											/* Loop back mode, rxd con to txd */
#define RK3568_SPI_CTRLR0_LBK_LB				_BITFIELD32L(RK3568_SPI_CTRLR0_LBK_OFFSET, 0x1)

/* Bit fields in SPI_ENR */
#define RK3568_SPI_ENR_ENABLE_SPI				0x1
#define RK3568_SPI_ENR_DISABLE_SPI				0x0

/* Bit fields in SPI_SER (slave enable) */
#define RK3568_SPI_SER_SS1						0x2
#define RK3568_SPI_SER_SS0						0x1

/* Bit fields in SPI_BYPASS */
#define RK3568_SPI_BYPASS_MODE_MASK				0x1
#define RK3568_SPI_BYPASS_ENABLE				0x1
#define RK3568_SPI_BYPASS_DISABLE				0x0

/* SPI interrupt-type definitions */
#define RK3568_SPI_INTR_TX_FINISH				0x80
#define RK3568_SPI_INTR_SS_IN_POSEDEGE			0x40
#define RK3568_SPI_INTR_SPI_TIMEOUT				0x20
#define RK3568_SPI_INTR_RX_FULL					0x10
#define RK3568_SPI_INTR_RXO						0x8
#define RK3568_SPI_INTR_RXU						0x4
#define RK3568_SPI_INTR_TXO						0x2
#define RK3568_SPI_INTR_TX_EMPTY				0x1
#define RK3568_SPI_ICR_CLEAR					0x7f

#endif /* __RK3568_SPI_H__ */
