#ifndef PL_SPI_DBG_H
#define PL_SPI_DBG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "xil_io.h"
#include "xparameters.h"

#define PL_SPI_DBG_DEFAULT_BASEADDR XPAR_SPI_MASTER_0_BASEADDR

#define PL_SPI_DBG_DEFAULT_BRAM_BASEADDR XPAR_AXI_BRAM_CTRL_2_S_AXI_BASEADDR

#define PL_SPI_DBG_IRQ_ID XPAR_FABRIC_SPI_MASTER_0_IRQ_INTR

typedef enum {
    PL_SPI_DBG_MODE_SINGLE = 0x1U,
    PL_SPI_DBG_MODE_MULTI = 0x2U,
    PL_SPI_DBG_MODE_FALL_THROUGH = 0x3U
} pl_spi_dbg_mode_t;

typedef struct {
    uint32_t base_addr;
    uint32_t bram_base_addr;
    pl_spi_dbg_mode_t mode;
    bool read_en;
} pl_spi_dbg_t;

typedef struct {
    bool tx_fifo_full;
    bool tx_fifo_empty;
    bool rx_fifo_full;
    bool rx_fifo_empty;
} pl_spi_dbg_status_t;

/* Core API (minimal set). */
void pl_spi_dbg_init_defaults(pl_spi_dbg_t *ctx);
void pl_spi_dbg_configure(pl_spi_dbg_t *ctx,
                         pl_spi_dbg_mode_t mode,
                         uint16_t clk_div_even_ge_2,
                         uint32_t timeout_ticks_ge_1,
                         bool read_en);
bool pl_spi_dbg_transfer_words(const pl_spi_dbg_t *ctx,
                              const uint32_t *tx_words,
                              size_t tx_word_count,
                              uint32_t *rx_words,
                              size_t rx_word_capacity,
                              uint32_t max_poll_iters);

void pl_spi_dbg_irq_clear(const pl_spi_dbg_t *ctx);

#endif
