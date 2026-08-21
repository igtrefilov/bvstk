#ifndef BVSTK_SD_INIT_H
#define BVSTK_SD_INIT_H

#include <stdint.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "hardware/pl/sd/bvstk_sd_init_regs.h"
#include "shared/base/bvstk_status.h"
#include "shared/interfaces/bvstk_clock.h"
#include "shared/interfaces/bvstk_mmio.h"
#include "shared/interfaces/bvstk_sync.h"

typedef struct {
    uint32_t irq;
    uint32_t csr;
    uint32_t response[BVSTK_SD_INIT_RESPONSE_COUNT];
    uint8_t hard_done;
    uint8_t soft_done;
} bvstk_sd_init_result_t;

typedef struct {
    bvstk_mmio_region_t control;
    bvstk_mmio_region_t response;
    bvstk_clock_t clock;
    bvstk_mutex_t mutex;
    uint8_t initialized;
} bvstk_sd_init_t;

bvstk_status_t bvstk_sd_init_init(bvstk_sd_init_t *driver,
                                   const bvstk_clock_t *clock,
                                   const bvstk_mutex_t *mutex);
void bvstk_sd_init_shutdown(bvstk_sd_init_t *driver);

bvstk_status_t bvstk_sd_init_run(bvstk_sd_init_t *driver,
                                 uint32_t timeout_ms,
                                 bvstk_sd_init_result_t *result);

#endif /* BVSTK_SD_INIT_H */
