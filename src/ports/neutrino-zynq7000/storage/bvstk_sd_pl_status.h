#ifndef BVSTK_SD_PL_STATUS_H
#define BVSTK_SD_PL_STATUS_H
#include <stdint.h>
#include <devctl.h>
typedef struct {
    uint32_t version, ready, fat_bits, first_lba, sectors, irq_count;
    uint32_t last_error, last_lba;
    char stage[64];
} bvstk_sd_pl_status_t;
#define BVSTK_SD_PL_NODE "/dev/bvstk-sd-pl-fat"
#define BVSTK_SD_PL_STATUS __DIOF(0x80, 9, bvstk_sd_pl_status_t)
#endif
