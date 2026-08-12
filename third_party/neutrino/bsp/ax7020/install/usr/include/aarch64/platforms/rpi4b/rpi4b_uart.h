/*
 *
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 *
 */
#ifndef __RPI4B_UART_H__
#define __RPI4B_UART_H__

/*
 * Minimal header for Raspberry Pi miniUART (AUX_MU) on BCM283x/BCM2711
 */

/* Base PHYS addr AUX block */
#define BCM283X_AUX_BASE      0xFE215000UL
/* miniUART +0x40 by start of AUX */
#define MINIUART_BASE         (BCM283X_AUX_BASE + 0x40UL)
#define MINIUART_SIZE         0x100UL

/* Offsets MINIUART_BASE */
#define AUX_MU_IO_REG         0x00UL  /* I/O Data */
#define AUX_MU_IER_REG        0x04UL  /* Interrupt Enable */
#define AUX_MU_IIR_REG        0x08UL  /* Interrupt Identify */
#define AUX_MU_LCR_REG        0x0CUL  /* Line Control */
#define AUX_MU_MCR_REG        0x10UL  /* Modem Control */
#define AUX_MU_LSR_REG        0x14UL  /* Line Status */
#define AUX_MU_MSR_REG        0x18UL  /* Modem Status */
#define AUX_MU_SCRATCH        0x1CUL  /* Scratch register */
#define AUX_MU_CNTL_REG       0x20UL  /* Control register */
#define AUX_MU_STAT_REG       0x24UL  /* Extra status */
#define AUX_MU_BAUD_REG       0x28UL  /* Baud rate */

/* Line Status */
#define AUX_MU_LSR_TX_EMPTY   (1 << 5)  /* Transmitter empty */
#define AUX_MU_LSR_RX_READY   (1 << 0)  /* Data ready */
#define AUX_MU_LSR_BREAK_IND  (1 << 4)   /* Break detect */

/* Line Control */
#define AUX_MU_LCR_8BIT       (1 << 0)  /* 8-bit mode */

/* Control */
#define AUX_MU_CNTL_TX_EN     (1 << 1)  /* Enable transmitter */
#define AUX_MU_CNTL_RX_EN     (1 << 0)  /* Enable receiver */


#endif /* __RPI4B_UART_H__ */
