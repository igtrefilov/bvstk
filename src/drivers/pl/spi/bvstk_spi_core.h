#ifndef BVSTK_SPI_CORE_H
#define BVSTK_SPI_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "shared/base/bvstk_status.h"
#include "shared/interfaces/bvstk_clock.h"
#include "shared/interfaces/bvstk_mmio.h"
#include "shared/interfaces/bvstk_sync.h"

typedef struct {
    uint8_t packets_mode;
    uint32_t timeout_ticks;
    uint16_t p_clk_div;
    bool read_enable;
} bvstk_spi_core_config_t;

typedef struct {
    bvstk_mmio_region_t master;
    bvstk_mmio_region_t bram;
    bvstk_clock_t clock;
    bvstk_mutex_t mutex;
    bvstk_spi_core_config_t config;
    uint8_t initialized;
} bvstk_spi_core_t;

bvstk_status_t bvstk_spi_core_init(bvstk_spi_core_t *core,
                                   const bvstk_spi_core_config_t *config,
                                   const bvstk_clock_t *clock,
                                   const bvstk_mutex_t *mutex);
void bvstk_spi_core_shutdown(bvstk_spi_core_t *core);

bvstk_status_t bvstk_spi_core_set_config(bvstk_spi_core_t *core,
                                         const bvstk_spi_core_config_t *config);
bvstk_status_t bvstk_spi_core_get_config(const bvstk_spi_core_t *core,
                                         bvstk_spi_core_config_t *config);
bvstk_status_t bvstk_spi_core_transfer(bvstk_spi_core_t *core,
                                       const uint32_t *tx_words,
                                       size_t tx_count,
                                       uint32_t *rx_words,
                                       size_t rx_capacity,
                                       uint32_t timeout_ms);

#endif /* BVSTK_SPI_CORE_H */
