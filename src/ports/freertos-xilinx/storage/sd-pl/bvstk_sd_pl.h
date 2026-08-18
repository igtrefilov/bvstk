#ifndef BVSTK_FREERTOS_SD_PL_H
#define BVSTK_FREERTOS_SD_PL_H

#include <stddef.h>
#include <stdint.h>

#include "drivers/pl/sd/bvstk_sd_controller.h"
#include "shared/base/bvstk_status.h"

bvstk_status_t bvstk_sd_pl_initialize(void);
void bvstk_sd_pl_shutdown(void);

int bvstk_sd_pl_is_ready(void);
bvstk_status_t bvstk_sd_pl_get_sector_count(uint32_t *sector_count);
bvstk_status_t bvstk_sd_pl_get_status(
    bvstk_sd_controller_status_t *status);
bvstk_status_t bvstk_sd_pl_read(uint32_t first_sector,
                                uint8_t *buffer,
                                size_t sector_count);
bvstk_status_t bvstk_sd_pl_write(uint32_t first_sector,
                                 const uint8_t *buffer,
                                 size_t sector_count);

bvstk_status_t bvstk_sd_pl_get_debug(
    bvstk_sd_controller_debug_t *debug);

#endif /* BVSTK_FREERTOS_SD_PL_H */
