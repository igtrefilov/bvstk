/*
 *
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 *
 */
#ifndef __RPI4B_PL011_H__
#define __RPI4B_PL011_H__

/*
 * PL011 UART (ARM PrimeCell UART) — base UART BCM283x / BCM2711
 */

#define PL011_BASE_ADDR       0xFE201000UL
#define PL011_REG_SIZE        0x1000UL

#define PL011_DR              0x00UL   /* Data Register */
#define PL011_RSRECR          0x04UL   /* Receive Status / Error Clear */
#define PL011_FR              0x18UL   /* Flag Register */
#define PL011_ILPR            0x20UL   /* IrDA Low-Power Counter (не используется) */
#define PL011_IBRD            0x24UL   /* Integer Baud rate divisor */
#define PL011_FBRD            0x28UL   /* Fractional Baud rate divisor */
#define PL011_LCRH            0x2CUL   /* Line Control Register */
#define PL011_CR              0x30UL   /* Control Register */
#define PL011_IFLS            0x34UL   /* Interrupt FIFO Level Select */
#define PL011_IMSC            0x38UL   /* Interrupt Mask Set/Clear */
#define PL011_RIS             0x3CUL   /* Raw Interrupt Status */
#define PL011_MIS             0x40UL   /* Masked Interrupt Status */
#define PL011_ICR             0x44UL   /* Interrupt Clear Register */
#define PL011_DMACR           0x48UL   /* DMA Control Register */

/* Flag Register */
#define PL011_FR_TXFF         (1 << 5)  /* Transmit FIFO Full */
#define PL011_FR_RXFE         (1 << 4)  /* Receive FIFO Empty */
#define PL011_FR_BUSY         (1 << 3)  /* UART Busy */
#define PL011_FR_TXFE         (1 << 7)  /* TX FIFO empty */
#define PL011_FR_RXFF         (1 << 6)  /* RX FIFO full */

/* Control Register */
#define PL011_CR_UARTEN       (1 << 0)  /* UART Enable */
#define PL011_CR_TXE          (1 << 8)  /* Transmit Enable */
#define PL011_CR_RXE          (1 << 9)  /* Receive Enable */

/* Line Control */
#define PL011_LCRH_FEN        (1 << 4)  /* Enable FIFOs */
#define PL011_LCRH_WLEN_8     (3 << 5)  /* Word length = 8 bits */


#endif // __RPI4B_PL011_H__
