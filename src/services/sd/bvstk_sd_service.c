#include "services/sd/bvstk_sd_service.h"

#include <string.h>

#define COMMAND_TIMEOUT_MS 5000U

static bvstk_status_t begin(bvstk_sd_service_t *s, bvstk_sd_result_t *r,
                            bool require_ready, bool filesystem)
{
    bvstk_status_t status;
    if (!r) return BVSTK_ERR_MALFORMED;
    memset(r, 0, sizeof(*r));
    r->mismatch_offset = UINT32_MAX;
    r->hardware.stage = "service";
    r->status = BVSTK_ERR_NOT_READY;
    if (!s || !s->driver || !s->driver->open) return r->status;
    status = bvstk_mutex_lock(&s->mutex, 1000);
    if (status != BVSTK_OK) { r->status = status; return status; }
    if (s->info.filesystem_owned != filesystem) {
        r->status = BVSTK_ERR_BUSY;
        r->hardware.stage = "filesystem owns card; raw commands disabled";
        bvstk_mutex_unlock(&s->mutex);
        return r->status;
    }
    if (require_ready && !s->info.ready) {
        bvstk_mutex_unlock(&s->mutex);
        return r->status;
    }
    return BVSTK_OK;
}

static bvstk_status_t finish(bvstk_sd_service_t *s, bvstk_sd_result_t *r,
                             bvstk_status_t status)
{
    r->status = status;
    /* A timeout, bad response or CRC means the transfer was aborted/reset.
     * Reinitialization is explicit; do not silently retry a media write. */
    if (status == BVSTK_ERR_IO || status == BVSTK_ERR_TIMEOUT ||
        status == BVSTK_ERR_INTERNAL) s->info.ready = false;
    s->info.last = *r;
    bvstk_mutex_unlock(&s->mutex);
    return status;
}

static bvstk_status_t address(bvstk_sd_service_t *s, uint32_t lba, uint32_t *arg)
{
    if (!s->info.block_addressed && lba > UINT32_MAX / SDM_BLOCK_SIZE)
        return BVSTK_ERR_RANGE;
    *arg = s->info.block_addressed ? lba : lba * SDM_BLOCK_SIZE;
    return BVSTK_OK;
}

static bvstk_status_t prepare_command(bvstk_sd_service_t *s, bvstk_sd_result_t *r)
{
    bvstk_status_t status;
    if (!s->reinitialize_between_commands || !s->command_since_init) return BVSTK_OK;
    ++r->reinitializations;
    /* Current black-box hardware completes a read but stalls the next
     * request. Use its documented full init sequence for an isolated command.
     * This is proactive preparation, NOT retry-after-timeout. */
    status = bvstk_sd_master_initialize(s->driver, 20000, &r->prepare_hardware);
    if (status != BVSTK_OK) return status;
    if (r->prepare_hardware.init_reply[5] != s->info.ocr) {
        r->prepare_hardware.stage = "OCR changed; explicit init required";
        return BVSTK_ERR_IO;
    }
    s->command_since_init = false;
    return BVSTK_OK;
}

static bvstk_status_t transfer(bvstk_sd_service_t *s, uint32_t lba,
    const uint8_t *write_block, uint8_t *read_block, bvstk_sd_result_t *r)
{
    uint32_t argument;
    bvstk_status_t status = address(s, lba, &argument);
    r->lba = lba;
    if (status != BVSTK_OK) { r->hardware.stage = "LBA range"; return status; }
    status = prepare_command(s, r);
    if (status != BVSTK_OK) return status;
    s->command_since_init = true;
    return bvstk_sd_master_command(s->driver, write_block ? 24 : 17,
        argument, write_block, read_block, COMMAND_TIMEOUT_MS, &r->hardware);
}

static bvstk_status_t compare(const uint8_t *expected, const uint8_t *received,
                              bvstk_sd_result_t *r)
{
    uint32_t i;
    for (i = 0; i < SDM_BLOCK_SIZE; ++i) {
        if (expected[i] != received[i]) {
            r->mismatch_offset = i;
            r->hardware.stage = "readback mismatch";
            return BVSTK_ERR_IO;
        }
    }
    return BVSTK_OK;
}

bvstk_status_t bvstk_sd_service_attach(bvstk_sd_service_t *s,
    bvstk_sd_master_t *driver, const bvstk_mutex_t *mutex,
    bool reinitialize_between_commands)
{
    if (!s || !driver || !driver->open || !mutex || !mutex->lock || !mutex->unlock)
        return BVSTK_ERR_MALFORMED;
    memset(s, 0, sizeof(*s));
    s->driver = driver;
    s->mutex = *mutex;
    s->reinitialize_between_commands = reinitialize_between_commands;
    s->info.last.status = BVSTK_ERR_NOT_READY;
    s->info.last.hardware.stage = "not initialized";
    s->info.last.mismatch_offset = UINT32_MAX;
    return BVSTK_OK;
}

bvstk_status_t bvstk_sd_service_info(bvstk_sd_service_t *s, bvstk_sd_card_info_t *info)
{
    bvstk_status_t status;
    if (!s || !s->driver || !info) return BVSTK_ERR_NOT_READY;
    status = bvstk_mutex_lock(&s->mutex, 1000);
    if (status != BVSTK_OK) return status;
    *info = s->info;
    bvstk_mutex_unlock(&s->mutex);
    return BVSTK_OK;
}

static bvstk_status_t initialize_locked(bvstk_sd_service_t *s,
    uint32_t timeout_ms, bvstk_sd_result_t *r)
{
    bvstk_status_t status;
    if (timeout_ms > 120000) return finish(s, r, BVSTK_ERR_RANGE);
    s->info.ready = false;
    s->info.ocr = 0;
    s->info.block_addressed = false;
    s->command_since_init = false;
    status = bvstk_sd_master_initialize(s->driver, timeout_ms, &r->hardware);
    if (status == BVSTK_OK) {
        s->info.ocr = r->hardware.init_reply[5];
        s->info.block_addressed = (s->info.ocr & UINT32_C(0x40000000)) != 0;
        if (!s->info.block_addressed && s->reinitialize_between_commands) {
            r->hardware.stage = "SDSC not qualified for isolated profile";
            status = BVSTK_ERR_UNSUPPORTED;
        } else if (!s->info.block_addressed) {
            /* SDSC needs an explicit 512-byte block length. SDHC/SDXC have
             * fixed 512-byte blocks; their documented read flow omits CMD16. */
            status = bvstk_sd_master_command(s->driver, 16, SDM_BLOCK_SIZE,
                NULL, NULL, COMMAND_TIMEOUT_MS, &r->write_hardware);
            s->command_since_init = true;
        }
        s->info.ready = status == BVSTK_OK;
    }
    return finish(s, r, status);
}

bvstk_status_t bvstk_sd_service_initialize(bvstk_sd_service_t *s,
    uint32_t timeout_ms, bvstk_sd_result_t *r)
{
    bvstk_status_t status = begin(s, r, false, false);
    return status == BVSTK_OK ? initialize_locked(s, timeout_ms, r) : status;
}

bvstk_status_t bvstk_sd_service_disk_claim(bvstk_sd_service_t *s,
    bvstk_sd_result_t *r)
{
    bvstk_status_t status = begin(s, r, false, false);
    if (status != BVSTK_OK) return status;
    s->info.filesystem_owned = true;
    return initialize_locked(s, 20000, r);
}

static bvstk_status_t read_block(bvstk_sd_service_t *s,
    uint32_t lba, uint8_t block[SDM_BLOCK_SIZE], bvstk_sd_result_t *r, bool filesystem)
{
    bvstk_status_t status = begin(s, r, true, filesystem);
    if (status != BVSTK_OK) return status;
    status = block ? transfer(s, lba, NULL, block, r) : BVSTK_ERR_MALFORMED;
    return finish(s, r, status);
}

static bvstk_status_t write_block(bvstk_sd_service_t *s,
    uint32_t lba, const uint8_t block[SDM_BLOCK_SIZE], bvstk_sd_result_t *r, bool filesystem)
{
    bvstk_status_t status = begin(s, r, true, filesystem);
    if (status != BVSTK_OK) return status;
    if (!block) return finish(s, r, BVSTK_ERR_MALFORMED);
    status = transfer(s, lba, block, NULL, r);
    r->write_hardware = r->hardware;
    if (status == BVSTK_OK) status = transfer(s, lba, NULL, s->received, r);
    if (status == BVSTK_OK) status = compare(block, s->received, r);
    return finish(s, r, status);
}

bvstk_status_t bvstk_sd_service_read(bvstk_sd_service_t *s,
    uint32_t lba, uint8_t block[SDM_BLOCK_SIZE], bvstk_sd_result_t *r)
{
    return read_block(s, lba, block, r, false);
}

bvstk_status_t bvstk_sd_service_write_verify(bvstk_sd_service_t *s,
    uint32_t lba, const uint8_t block[SDM_BLOCK_SIZE], bvstk_sd_result_t *r)
{
    return write_block(s, lba, block, r, false);
}

bvstk_status_t bvstk_sd_service_disk_read(bvstk_sd_service_t *s,
    uint32_t lba, uint8_t block[SDM_BLOCK_SIZE], bvstk_sd_result_t *r)
{
    return read_block(s, lba, block, r, true);
}

bvstk_status_t bvstk_sd_service_disk_write(bvstk_sd_service_t *s,
    uint32_t lba, const uint8_t block[SDM_BLOCK_SIZE], bvstk_sd_result_t *r)
{
    return write_block(s, lba, block, r, true);
}

static bvstk_status_t simple_command(bvstk_sd_service_t *s, uint8_t cmd,
                                     bvstk_sd_result_t *r)
{
    bvstk_status_t status = begin(s, r, true, false);
    if (status != BVSTK_OK) return status;
    status = prepare_command(s, r);
    if (status != BVSTK_OK) return finish(s, r, status);
    s->command_since_init = true;
    status = bvstk_sd_master_command(s->driver, cmd, cmd == 16 ? SDM_BLOCK_SIZE : 0,
        NULL, NULL, COMMAND_TIMEOUT_MS, &r->hardware);
    return finish(s, r, status);
}

bvstk_status_t bvstk_sd_service_card_status(bvstk_sd_service_t *s, bvstk_sd_result_t *r)
{
    return simple_command(s, 13, r);
}

bvstk_status_t bvstk_sd_service_block_length(bvstk_sd_service_t *s, bvstk_sd_result_t *r)
{
    return simple_command(s, 16, r);
}

void bvstk_sd_service_pattern(uint8_t block[SDM_BLOCK_SIZE], uint32_t lba, uint32_t seed)
{
    uint32_t state = seed ^ lba ^ UINT32_C(0x9e3779b9);
    size_t i;
    if (!state) state = 1;
    for (i = 0; i < SDM_BLOCK_SIZE; ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        block[i] = (uint8_t)state;
    }
    for (i = 0; i < 4; ++i) {
        block[i] = (uint8_t)(lba >> (i * 8));
        block[i + 4] = (uint8_t)(seed >> (i * 8));
    }
}

bvstk_status_t bvstk_sd_service_test(bvstk_sd_service_t *s,
    uint32_t lba, uint32_t count, uint32_t seed, bvstk_sd_result_t *r)
{
    uint32_t i, unused;
    bvstk_status_t status = begin(s, r, true, false);
    if (status != BVSTK_OK) return status;
    if (!count || count > 8 || lba > UINT32_MAX - (count - 1) ||
        address(s, lba + count - 1, &unused) != BVSTK_OK)
        return finish(s, r, BVSTK_ERR_RANGE);
    /* Write all sectors before reading any, so an address-aliasing failure
     * cannot pass a same-sector immediate readback test. */
    for (i = 0; i < count; ++i) {
        bvstk_sd_service_pattern(s->expected, lba + i, seed);
        status = transfer(s, lba + i, s->expected, NULL, r);
        r->write_hardware = r->hardware;
        if (status != BVSTK_OK) return finish(s, r, status);
    }
    for (i = 0; i < count; ++i) {
        status = transfer(s, lba + i, NULL, s->received, r);
        if (status != BVSTK_OK) return finish(s, r, status);
        bvstk_sd_service_pattern(s->expected, lba + i, seed);
        status = compare(s->expected, s->received, r);
        if (status != BVSTK_OK) return finish(s, r, status);
    }
    return finish(s, r, BVSTK_OK);
}
