#ifndef BVSTK_SD_MASTER_REGS_H
#define BVSTK_SD_MASTER_REGS_H

/* SPI_page_assets/page_68812882: extended SD command/DMA interface.
 * These are byte offsets, not the old sd_spi_controller register map.
 * The documentation names completion events but does not specify their IRQ
 * bit positions. Use the fabric interrupt and fresh responses, not guessed
 * masks from the older FIFO interface. */
enum {
    SDM_CSR = 0x00, SDM_IRQ = 0x04, SDM_PACKET = 0x08,
    SDM_DELAY = 0x0c, SDM_INIT = 0x14, SDM_DIVIDER = 0x18,
    SDM_RESET = 1, SDM_START_READ = 6, SDM_START_COMMAND = 14,
    SDM_ACK = 1, SDM_FALL_THROUGH = 3, SDM_INIT_VALUE = 0x193,
    SDM_TX_CONTROL = 0x00, SDM_TX_STATUS = 0x04,
    SDM_TX_SOURCE = 0x18, SDM_TX_LENGTH = 0x28,
    SDM_RX_CONTROL = 0x30, SDM_RX_STATUS = 0x34,
    SDM_RX_DESTINATION = 0x48, SDM_RX_LENGTH = 0x58,
    SDM_DMA_RUN = 1, SDM_DMA_RESET = 4, SDM_DMA_HALTED = 1,
    SDM_DMA_IDLE = 2, SDM_DMA_ERRORS = 0x770,
    SDM_DMA_IOC = 0x1000, SDM_DMA_IRQS = 0x7000,
    SDM_MB_CMD13 = 0x34, SDM_MB_CMD16 = 0x38,
    SDM_MB_CMD17 = 0x3c, SDM_MB_CMD24 = 0x44,
    SDM_TX_AREA = 0x400, SDM_RX_AREA = 0x1000,
    SDM_BLOCK_SIZE = 512, SDM_RX_BYTES = 514, SDM_TX_BYTES = 522,
    SDM_INIT_WORDS = 7, SDM_BRAM_MIN = 0x1208
};

#endif
