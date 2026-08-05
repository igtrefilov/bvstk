#ifndef BVSTK_SPI_H
#define BVSTK_SPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "xparameters.h"
#include "xil_io.h"
#include "xil_types.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "hardware/pl/spi/bvstk_spi_regs.h"

#define SPI_TASK_STACK_SIZE     (768U)
#define SPI_TASK_PRIORITY       (tskIDLE_PRIORITY + 1U)

#define SPI_BASEADDR            BVSTK_SPI_MASTER_BASE
#define SPI_BRAM_BASEADDR       BVSTK_SPI_BRAM_BASE
#define SPI_IRQ_INTR            BVSTK_IRQ_SPI_MASTER
#define SPI_HAS_IRQ             1

/* SPI register map. */
#define SPI_CSR_REG_OFFSET      BVSTK_SPI_CSR_OFFSET
#define SPI_IRQ_REG_OFFSET      BVSTK_SPI_IRQ_OFFSET
#define SPI_PACKET_OFFSET       BVSTK_SPI_PACKET_OFFSET
#define SPI_TIMEOUT_OFFSET      BVSTK_SPI_TIMEOUT_OFFSET
#define SPI_TX_FIFO_OFFSET      BVSTK_SPI_TX_FIFO_OFFSET
#define SPI_SD_INI_OFFSET       BVSTK_SPI_SD_INI_OFFSET
#define SPI_SIG_REG_OFFSET      BVSTK_SPI_SIGNATURE_OFFSET

#define SPI_MODE_SINGLE         BVSTK_SPI_MODE_SINGLE
#define SPI_MODE_MULTI          BVSTK_SPI_MODE_MULTI
#define SPI_MODE_FALLTHROUGH    BVSTK_SPI_MODE_FALLTHROUGH

typedef struct {
    uint8_t packets_mode;     /* 01 single, 10 multi, 11 fall-through */
    uint32_t timeout_ticks;   /* >= 1 */
    uint16_t p_clk_div;       /* >= 2 and even */
    bool read_en;             /* true: full duplex */
} spi_runtime_cfg_t;

void start_spi(void);

void spi_set_cfg(const spi_runtime_cfg_t *cfg);
void spi_get_cfg(spi_runtime_cfg_t *out);

bool spi_transfer_words(const uint32_t *tx_words,
                        size_t tx_count,
                        uint32_t *rx_words,
                        size_t rx_capacity,
                        TickType_t timeout_ticks);

#endif
