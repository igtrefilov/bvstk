#ifndef BVSTK_SD_VOLUME_H
#define BVSTK_SD_VOLUME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "shared/base/bvstk_status.h"

typedef struct {
    uint32_t first_lba, sectors;
    uint8_t fat_bits;
} bvstk_sd_volume_t;

typedef bvstk_status_t (*bvstk_sd_volume_read_t)(void *context,
    uint32_t lba, uint8_t sector[512]);

/* Read-only discovery: FAT12/16/32 superfloppy or first valid FAT primary MBR
 * partition. No GPT, extended partitions, exFAT, repair or formatting.
 * BPB geometry bounds the exposed volume; it is NOT physical card capacity. */
bvstk_status_t bvstk_sd_volume_probe(bvstk_sd_volume_read_t read, void *context,
    bvstk_sd_volume_t *volume);
bool bvstk_sd_volume_map(const bvstk_sd_volume_t *volume, uint32_t sector,
    size_t count, uint32_t *physical_lba);

#endif
