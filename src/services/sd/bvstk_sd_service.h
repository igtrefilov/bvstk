#ifndef BVSTK_SD_SERVICE_H
#define BVSTK_SD_SERVICE_H

#include "drivers/pl/sd/bvstk_sd_master.h"
#include "shared/interfaces/bvstk_sync.h"

typedef struct {
    bvstk_sd_master_report_t hardware;
    bvstk_sd_master_report_t write_hardware;
    bvstk_sd_master_report_t prepare_hardware;
    uint32_t lba, mismatch_offset, reinitializations;
    bvstk_status_t status;
} bvstk_sd_result_t;

typedef struct {
    bool ready, block_addressed, filesystem_owned;
    uint32_t ocr;
    bvstk_sd_result_t last;
} bvstk_sd_card_info_t;

typedef struct {
    bvstk_sd_master_t *driver;
    bvstk_mutex_t mutex;
    bvstk_sd_card_info_t info;
    bool reinitialize_between_commands, command_since_init;
    uint8_t expected[SDM_BLOCK_SIZE], received[SDM_BLOCK_SIZE];
} bvstk_sd_service_t;

/* Call once, before publishing the service to other tasks. No card I/O. */
bvstk_status_t bvstk_sd_service_attach(bvstk_sd_service_t *service,
    bvstk_sd_master_t *driver, const bvstk_mutex_t *mutex,
    bool reinitialize_between_commands);
/* The isolated compatibility profile is qualified for block-addressed SDv2
 * only. It reinitializes BEFORE a new command, never retries a failed write.
 * Errors still require an explicit public initialize(). */
bvstk_status_t bvstk_sd_service_info(bvstk_sd_service_t *service,
    bvstk_sd_card_info_t *info);
bvstk_status_t bvstk_sd_service_initialize(bvstk_sd_service_t *service,
    uint32_t timeout_ms, bvstk_sd_result_t *result);
bvstk_status_t bvstk_sd_service_read(bvstk_sd_service_t *service,
    uint32_t lba, uint8_t block[SDM_BLOCK_SIZE], bvstk_sd_result_t *result);
/* Always reads back and compares all 512 bytes under the same lock. */
bvstk_status_t bvstk_sd_service_write_verify(bvstk_sd_service_t *service,
    uint32_t lba, const uint8_t block[SDM_BLOCK_SIZE], bvstk_sd_result_t *result);
bvstk_status_t bvstk_sd_service_card_status(bvstk_sd_service_t *service,
    bvstk_sd_result_t *result);
/* Explicit documented CMD16 diagnostic; the only supported size is 512. */
bvstk_status_t bvstk_sd_service_block_length(bvstk_sd_service_t *service,
    bvstk_sd_result_t *result);
/* Exclusive lifetime claim for the filesystem. Even a failed initialization
 * keeps raw commands blocked. Recovery/removal requires a system restart, so
 * stale FatFs handles can never resume I/O on a newly initialized card. */
bvstk_status_t bvstk_sd_service_disk_claim(bvstk_sd_service_t *service,
    bvstk_sd_result_t *result);
bvstk_status_t bvstk_sd_service_disk_read(bvstk_sd_service_t *service,
    uint32_t lba, uint8_t block[SDM_BLOCK_SIZE], bvstk_sd_result_t *result);
bvstk_status_t bvstk_sd_service_disk_write(bvstk_sd_service_t *service,
    uint32_t lba, const uint8_t block[SDM_BLOCK_SIZE], bvstk_sd_result_t *result);
/* DESTRUCTIVE: write 1..8 distinct sectors, then verify the whole range.
 * No capacity discovery: caller must supply an existing, expendable range.
 * No automatic write retry, erase, formatting, or filesystem integration. */
bvstk_status_t bvstk_sd_service_test(bvstk_sd_service_t *service,
    uint32_t lba, uint32_t count, uint32_t seed, bvstk_sd_result_t *result);
void bvstk_sd_service_pattern(uint8_t block[SDM_BLOCK_SIZE],
    uint32_t lba, uint32_t seed);

#endif
