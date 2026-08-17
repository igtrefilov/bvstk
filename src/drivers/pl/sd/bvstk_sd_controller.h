#ifndef BVSTK_SD_CONTROLLER_H
#define BVSTK_SD_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "shared/base/bvstk_status.h"
#include "shared/interfaces/bvstk_clock.h"
#include "shared/interfaces/bvstk_mmio.h"
#include "shared/interfaces/bvstk_sync.h"

#define BVSTK_SD_BLOCK_SIZE ((size_t)BVSTK_SD_SECTOR_SIZE)
#define BVSTK_SD_BUFFER_WORDS (BVSTK_SD_BLOCK_SIZE / sizeof(uint32_t))

typedef struct {
    uint32_t timeout_ms;
    uint16_t clock_div;
} bvstk_sd_controller_config_t;

typedef struct {
    bool busy;
    bool done;
    bool error;
    bool initialized;
    bool high_capacity;
    uint8_t error_code;
    uint8_t last_r1;
    uint8_t current_cmd;
} bvstk_sd_controller_status_t;

#define BVSTK_SD_DEBUG_TRACE_LENGTH 32U

typedef struct {
    uint32_t state;
    uint32_t counters;
    uint32_t last_cmd_hi;
    uint32_t last_cmd_lo;
    uint32_t response;
    uint32_t acmd41;
    uint32_t cmd55;
    uint32_t pins;
    uint32_t cmd55_hi;
    uint32_t cmd55_lo;
    uint32_t cmd41_hi;
    uint32_t cmd41_lo;
    uint32_t last_byte;
    uint32_t diag;
    uint32_t trace[BVSTK_SD_DEBUG_TRACE_LENGTH];
} bvstk_sd_controller_debug_t;

typedef struct {
    bvstk_mmio_region_t registers;
    bvstk_clock_t clock;
    bvstk_mutex_t mutex;
    bvstk_sd_controller_config_t config;
    bool initialized;
} bvstk_sd_controller_t;

bvstk_status_t bvstk_sd_controller_init(
    bvstk_sd_controller_t *controller,
    const bvstk_sd_controller_config_t *config,
    const bvstk_clock_t *clock,
    const bvstk_mutex_t *mutex);

void bvstk_sd_controller_shutdown(bvstk_sd_controller_t *controller);

bvstk_status_t bvstk_sd_controller_get_status(
    const bvstk_sd_controller_t *controller,
    bvstk_sd_controller_status_t *status);

bvstk_status_t bvstk_sd_controller_get_debug(
    const bvstk_sd_controller_t *controller,
    bvstk_sd_controller_debug_t *debug);

bvstk_status_t bvstk_sd_controller_initialize_card(
    bvstk_sd_controller_t *controller,
    uint32_t timeout_ms);

bvstk_status_t bvstk_sd_controller_read(
    bvstk_sd_controller_t *controller,
    uint32_t first_sector,
    uint8_t *buffer,
    size_t sector_count,
    uint32_t timeout_ms);

bvstk_status_t bvstk_sd_controller_write(
    bvstk_sd_controller_t *controller,
    uint32_t first_sector,
    const uint8_t *buffer,
    size_t sector_count,
    uint32_t timeout_ms);

#endif /* BVSTK_SD_CONTROLLER_H */
