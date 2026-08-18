#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.h"

#include <stdbool.h>
#include <string.h>

#include "drivers/pl/sd/bvstk_sd_controller.h"
#include "ports/freertos-xilinx/os/bvstk_sync_freertos.h"
#include "xil_printf.h"

enum {
    /* The PL controller now allows 4096 ACMD41 polls at the 400 kHz init
     * clock; leave enough time for the complete initialization transaction. */
    BVSTK_SD_PL_INIT_TIMEOUT_MS = 20000U,
    BVSTK_SD_PL_TRANSFER_TIMEOUT_MS = 5000U
};

static bvstk_sd_controller_t s_controller;
static bvstk_freertos_mutex_t s_mutex;
static bool s_mutex_ready;
static bool s_controller_ready;
static bool s_card_ready;
static uint32_t s_sector_count;
static bool s_sector_count_ready;

static void print_debug_snapshot(void);

static uint16_t read_le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
}

static uint32_t read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}

static bool parse_fat_volume_size(const uint8_t *sector, uint32_t *count)
{
    uint32_t total;

    if (sector == NULL || count == NULL ||
        sector[510] != 0x55U || sector[511] != 0xaaU ||
        read_le16(sector + 11U) != 512U) {
        return false;
    }
    total = read_le16(sector + 19U);
    if (total == 0U) {
        total = read_le32(sector + 32U);
    }
    if (total == 0U) {
        return false;
    }
    *count = total;
    return true;
}

static void discover_volume_size(void)
{
    uint8_t sector[BVSTK_SD_SECTOR_SIZE];
    uint32_t partition_starts[4] = {0U, 0U, 0U, 0U};
    unsigned partition;

    s_sector_count = 0U;
    s_sector_count_ready = false;
    if (bvstk_sd_controller_read(&s_controller,
                                  0U,
                                  sector,
                                  1U,
                                  BVSTK_SD_PL_TRANSFER_TIMEOUT_MS) != BVSTK_OK) {
        return;
    }
    if (parse_fat_volume_size(sector, &s_sector_count)) {
        s_sector_count_ready = true;
        return;
    }

    /* Common MBR layout: inspect the four primary partition entries. */
    for (partition = 0U; partition < 4U; ++partition) {
        const uint8_t *entry = sector + 446U + partition * 16U;
        if (entry[4] != 0U) {
            partition_starts[partition] = read_le32(entry + 8U);
        }
    }
    for (partition = 0U; partition < 4U; ++partition) {
        uint32_t partition_start = partition_starts[partition];
        if (partition_start == 0U) continue;
        if (bvstk_sd_controller_read(&s_controller,
                                     partition_start,
                                     sector,
                                     1U,
                                     BVSTK_SD_PL_TRANSFER_TIMEOUT_MS) == BVSTK_OK &&
            parse_fat_volume_size(sector, &s_sector_count)) {
            s_sector_count_ready = true;
            return;
        }
    }
}

bvstk_status_t bvstk_sd_pl_initialize(void)
{
    bvstk_sd_controller_config_t config = {
        .timeout_ms = BVSTK_SD_PL_TRANSFER_TIMEOUT_MS,
        .clock_div = BVSTK_SD_CLOCK_DIV_DEFAULT
    };
    bvstk_status_t status;

    if (s_card_ready) {
        return BVSTK_OK;
    }
    if (!s_mutex_ready) {
        status = bvstk_freertos_mutex_init(&s_mutex);
        if (status != BVSTK_OK) {
            return status;
        }
        s_mutex_ready = true;
    }
    if (!s_controller_ready) {
        status = bvstk_sd_controller_init(&s_controller,
                                          &config,
                                          NULL,
                                          &s_mutex.public_mutex);
        if (status != BVSTK_OK) {
            bvstk_freertos_mutex_destroy(&s_mutex);
            s_mutex_ready = false;
            return status;
        }
        s_controller_ready = true;
    }

    status = bvstk_sd_controller_initialize_card(&s_controller,
                                                  BVSTK_SD_PL_INIT_TIMEOUT_MS);
    if (status != BVSTK_OK) {
        bvstk_sd_controller_status_t card_status;
        if (bvstk_sd_controller_get_status(&s_controller, &card_status) == BVSTK_OK) {
            xil_printf("SD-PL: init failed code=0x%02x r1=0x%02x cmd=0x%02x\r\n",
                       card_status.error_code,
                       card_status.last_r1,
                       card_status.current_cmd);
        } else {
            xil_printf("SD-PL: init failed status=%d\r\n", (int)status);
        }
        print_debug_snapshot();
        return status;
    }
    s_card_ready = true;
    discover_volume_size();
    xil_printf("SD-PL: initialized\r\n");
    return BVSTK_OK;
}

void bvstk_sd_pl_shutdown(void)
{
    if (s_controller_ready) {
        bvstk_sd_controller_shutdown(&s_controller);
    }
    if (s_mutex_ready) {
        bvstk_freertos_mutex_destroy(&s_mutex);
    }
    memset(&s_controller, 0, sizeof(s_controller));
    memset(&s_mutex, 0, sizeof(s_mutex));
    s_controller_ready = false;
    s_mutex_ready = false;
    s_card_ready = false;
    s_sector_count = 0U;
    s_sector_count_ready = false;
}

int bvstk_sd_pl_is_ready(void)
{
    return s_card_ready ? 1 : 0;
}

bvstk_status_t bvstk_sd_pl_get_sector_count(uint32_t *sector_count)
{
    if (!s_card_ready) {
        return BVSTK_ERR_NOT_READY;
    }
    if (sector_count == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    if (!s_sector_count_ready) {
        return BVSTK_ERR_UNSUPPORTED;
    }
    *sector_count = s_sector_count;
    return BVSTK_OK;
}

bvstk_status_t bvstk_sd_pl_get_status(
    bvstk_sd_controller_status_t *status)
{
    bvstk_status_t result;

    if (!s_controller_ready || !s_mutex_ready) {
        return BVSTK_ERR_NOT_READY;
    }
    result = bvstk_mutex_lock(&s_controller.mutex,
                              BVSTK_SD_PL_TRANSFER_TIMEOUT_MS);
    if (result != BVSTK_OK) {
        return result;
    }
    result = bvstk_sd_controller_get_status(&s_controller, status);
    bvstk_mutex_unlock(&s_controller.mutex);
    return result;
}

bvstk_status_t bvstk_sd_pl_read(uint32_t first_sector,
                                uint8_t *buffer,
                                size_t sector_count)
{
    if (!s_card_ready) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_sd_controller_read(&s_controller,
                                    first_sector,
                                    buffer,
                                    sector_count,
                                    BVSTK_SD_PL_TRANSFER_TIMEOUT_MS);
}

bvstk_status_t bvstk_sd_pl_write(uint32_t first_sector,
                                 const uint8_t *buffer,
                                 size_t sector_count)
{
    if (!s_card_ready) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_sd_controller_write(&s_controller,
                                     first_sector,
                                     buffer,
                                     sector_count,
                                     BVSTK_SD_PL_TRANSFER_TIMEOUT_MS);
}

bvstk_status_t bvstk_sd_pl_get_debug(bvstk_sd_controller_debug_t *debug)
{
    bvstk_status_t status;

    if (!s_controller_ready || !s_mutex_ready) {
        return BVSTK_ERR_NOT_READY;
    }
    status = bvstk_mutex_lock(&s_controller.mutex,
                              BVSTK_SD_PL_TRANSFER_TIMEOUT_MS);
    if (status != BVSTK_OK) {
        return status;
    }
    status = bvstk_sd_controller_get_debug(&s_controller, debug);
    bvstk_mutex_unlock(&s_controller.mutex);
    return status;
}

static void print_debug_snapshot(void)
{
    bvstk_sd_controller_debug_t debug;

    if (bvstk_sd_pl_get_debug(&debug) != BVSTK_OK) {
        xil_printf("SD-PL: DBG unavailable\r\n");
        return;
    }
    xil_printf("SD-PL: DBG state=%08x cnt=%08x acmd=%08x c55=%08x rsp=%08x seq=%08x pins=%08x last=%08x diag=%08x\r\n",
               debug.state,
               debug.counters,
               debug.acmd41,
               debug.cmd55,
               debug.response,
               debug.cmd55,
               debug.pins,
               debug.last_byte,
               debug.diag);
    xil_printf("SD-PL: DBG CMD55=%08x%04x CMD41=%08x%04x LAST=%08x%04x\r\n",
               debug.cmd55_hi,
               debug.cmd55_lo & 0xffffU,
               debug.cmd41_hi,
               debug.cmd41_lo & 0xffffU,
               debug.last_cmd_hi,
               debug.last_cmd_lo & 0xffffU);
}
