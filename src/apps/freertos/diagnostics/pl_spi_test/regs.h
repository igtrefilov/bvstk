#ifndef PL_SPI_TEST_REGS_H
#define PL_SPI_TEST_REGS_H

#include <stdint.h>

/* Offsets from the saved SPI page. Base addresses come ONLY from the BSP. */
enum {
    SPI_CSR = 0x00U, SPI_IRQ = 0x04U, SPI_PACKET = 0x08U,
    SPI_DELAY = 0x0CU, SPI_TX_FIFO = 0x10U,
    SPI_SD_INIT = 0x14U, SPI_DIVIDER = 0x18U
};
enum {
    SPI_RESET = 1U, SPI_START = 2U, SPI_READ = 4U,
    SPI_SD_COMMAND_START = 8U, SPI_ACK = 1U,
    SPI_SINGLE = 1U, SPI_COUNTED = 2U, SPI_STREAM = 3U,
    SPI_DOC_TX_FULL = 1U, SPI_DOC_TX_EMPTY = 2U,
    SPI_DOC_RX_FULL = 4U, SPI_DOC_RX_EMPTY = 8U,
    SPI_DOC_IRQ_TX_FULL = 1U, SPI_DOC_IRQ_RX_FULL = 2U,
    SPI_DOC_IRQ_END = 4U
};

/* Explicit integration profile for the extended SD interface. These masks
 * describe the current ABI; they are NOT a claim that the older page agrees.
 * Legacy FIFO tests retain SPI_DOC_IRQ_END, including any resulting failure. */
enum {
    SPI_SD_IRQ_COMMAND = 1U, SPI_SD_IRQ_SOFT_INIT = 2U,
    SPI_SD_IRQ_HARD_INIT = 4U, SPI_SD_IRQ_TRANSACTION = 8U,
    SPI_SD_IRQ_RX_FULL = 16U, SPI_SD_IRQ_TX_FULL = 32U,
    SPI_SD_CLOCK_ENABLE = 1U, SPI_SD_SEQUENCE_ENABLE = 2U
};

/* AXI DMA simple mode, checked against the local Xilinx BSP in driver.c. */
enum {
    DMA_TX_CONTROL = 0x00U, DMA_TX_STATUS = 0x04U,
    DMA_TX_SOURCE = 0x18U, DMA_TX_LENGTH = 0x28U,
    DMA_RX_CONTROL = 0x30U, DMA_RX_STATUS = 0x34U,
    DMA_RX_DESTINATION = 0x48U, DMA_RX_LENGTH = 0x58U,
    DMA_RUN = 1U, DMA_RESET = 4U, DMA_HALTED = 1U, DMA_IDLE = 2U,
    DMA_ERRORS = 0x770U, DMA_IOC = 0x1000U, DMA_IRQS = 0x7000U
};

/* Command response offsets from CMDS (новые).xlsx, relative to SPI BRAM. */
enum {
    MB_CMD0 = 0x00U, MB_CMD8_DATA = 0x04U, MB_CMD8_R1 = 0x08U,
    MB_ACMD41 = 0x0CU, MB_CMD55 = 0x10U,
    MB_CMD58_OCR = 0x14U, MB_CMD58_R1 = 0x18U,
    MB_CMD13 = 0x34U, MB_CMD16 = 0x38U, MB_CMD17 = 0x3CU,
    MB_CMD24 = 0x44U, MB_CMD32 = 0x5CU, MB_CMD33 = 0x60U,
    MB_CMD38 = 0x64U, SPI_INIT_WORDS = 7U
};

/* Separate command mailbox, DMA source and DMA destination. */
#define SPI_TX_AREA       0x0400U
#define SPI_RX_AREA       0x1000U
#define SPI_RX_CAPACITY   1024U
#define SPI_BLOCK_BYTES   512U
#define SPI_BLOCK_RX      514U
#define SPI_BLOCK_TX      522U
#define SPI_SENTINEL      UINT32_C(0xA5963CC3)
#define SPI_GUARD         UINT32_C(0x69C35AA5)
#define SPI_PACKET_VALUE(mode, count) (((uint32_t)(count) << 2) | (mode))

#endif
