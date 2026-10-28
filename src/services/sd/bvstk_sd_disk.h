#ifndef BVSTK_SD_DISK_H
#define BVSTK_SD_DISK_H
#include "services/sd/bvstk_sd_service.h"
#include "services/sd/bvstk_sd_volume.h"
/* Caller owns serialization and zero-initializes this lifetime object. */
typedef struct {
    bvstk_sd_service_t *service;
    bvstk_sd_volume_t volume;
    bool attempted, ready;
} bvstk_sd_disk_t;
bvstk_status_t bvstk_sd_disk_open(bvstk_sd_disk_t *, bvstk_sd_service_t *);
bvstk_status_t bvstk_sd_disk_read(bvstk_sd_disk_t *, uint32_t, uint8_t *, size_t);
bvstk_status_t bvstk_sd_disk_write(bvstk_sd_disk_t *, uint32_t, const uint8_t *, size_t);
#endif
