#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.h"

#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "drivers/pl/sd/bvstk_sd_controller.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "hardware/pl/sd/bvstk_sd_regs.h"
#include "ports/freertos-xilinx/os/bvstk_sync_freertos.h"
#include "portmacro.h"
#include "semphr.h"
#include "task.h"
#include "xil_io.h"
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
static bool s_irq_ready;
static SemaphoreHandle_t s_completion;
static uint32_t s_sector_count;
static bool s_sector_count_ready;

static void sd_pl_irq_handler(void *context)
{
    uint32_t events;
    BaseType_t higher_priority_task_woken = pdFALSE;

    (void)context;
    if (s_completion == NULL) {
        return;
    }
    events = Xil_In32(BVSTK_SD_CONTROLLER_BASE +
                      BVSTK_SD_IRQ_STATUS_OFFSET);
    if (events == 0U) {
        return;
    }
    Xil_Out32(BVSTK_SD_CONTROLLER_BASE + BVSTK_SD_IRQ_STATUS_OFFSET,
              events);
    (void)xSemaphoreGiveFromISR(s_completion,
                                &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static bvstk_status_t sd_pl_prepare_completion(void *context)
{
    (void)context;
    if (s_completion == NULL || !s_irq_ready) {
        return BVSTK_ERR_NOT_READY;
    }
    while (xSemaphoreTake(s_completion, 0U) == pdTRUE) {
    }
    Xil_Out32(BVSTK_SD_CONTROLLER_BASE + BVSTK_SD_IRQ_STATUS_OFFSET,
              BVSTK_SD_IRQ_TRANSFER_DONE | BVSTK_SD_IRQ_ERROR);
    Xil_Out32(BVSTK_SD_CONTROLLER_BASE + BVSTK_SD_IRQ_ENABLE_OFFSET,
              BVSTK_SD_IRQ_TRANSFER_DONE | BVSTK_SD_IRQ_ERROR);
    return BVSTK_OK;
}

static bvstk_status_t sd_pl_wait_completion(void *context,
                                            uint32_t timeout_ms)
{
    TickType_t ticks;

    (void)context;
    if (s_completion == NULL || !s_irq_ready) {
        return BVSTK_ERR_NOT_READY;
    }
    ticks = pdMS_TO_TICKS(timeout_ms);
    if (timeout_ms != 0U && ticks == 0U) {
        ticks = 1U;
    }
    return xSemaphoreTake(s_completion, ticks) == pdTRUE
               ? BVSTK_OK
               : BVSTK_ERR_TIMEOUT;
}

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
#if !BVSTK_PL_SD_CAN_BLOCK_IO
    return BVSTK_ERR_UNSUPPORTED;
#else
    bvstk_sd_controller_config_t config = {
        .timeout_ms = BVSTK_SD_PL_TRANSFER_TIMEOUT_MS,
        .clock_div = BVSTK_SD_CLOCK_DIV_DEFAULT,
        .completion = {
            .context = NULL,
            .prepare = sd_pl_prepare_completion,
            .wait = sd_pl_wait_completion
        }
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
    if (s_completion == NULL) {
        s_completion = xSemaphoreCreateBinary();
        if (s_completion == NULL) {
            bvstk_freertos_mutex_destroy(&s_mutex);
            s_mutex_ready = false;
            return BVSTK_ERR_INTERNAL;
        }
    }
    if (!s_controller_ready) {
        status = bvstk_sd_controller_init(&s_controller,
                                          &config,
                                          NULL,
                                          &s_mutex.public_mutex);
        if (status != BVSTK_OK) {
            bvstk_freertos_mutex_destroy(&s_mutex);
            vSemaphoreDelete(s_completion);
            s_completion = NULL;
            s_mutex_ready = false;
            return status;
        }
        s_controller_ready = true;
    }

    if (!s_irq_ready) {
        if (xPortInstallInterruptHandler(BVSTK_IRQ_SD_CONTROLLER,
                                         sd_pl_irq_handler,
                                         NULL) != pdPASS) {
            bvstk_sd_controller_shutdown(&s_controller);
            s_controller_ready = false;
            vSemaphoreDelete(s_completion);
            s_completion = NULL;
            bvstk_freertos_mutex_destroy(&s_mutex);
            s_mutex_ready = false;
            return BVSTK_ERR_INTERNAL;
        }
        s_irq_ready = true;
        vPortEnableInterrupt(BVSTK_IRQ_SD_CONTROLLER);
    }

    status = bvstk_sd_controller_initialize_card(&s_controller,
                                                  BVSTK_SD_PL_INIT_TIMEOUT_MS);
    if (status != BVSTK_OK) {
        xil_printf("SD-PL: init failed status=%d\r\n", (int)status);
        return status;
    }
    s_card_ready = true;
    discover_volume_size();
    xil_printf("SD-PL: initialized\r\n");
    return BVSTK_OK;
#endif
}

void bvstk_sd_pl_shutdown(void)
{
#if !BVSTK_PL_SD_CAN_BLOCK_IO
    return;
#else
    if (s_irq_ready) {
        vPortDisableInterrupt(BVSTK_IRQ_SD_CONTROLLER);
        Xil_Out32(BVSTK_SD_CONTROLLER_BASE + BVSTK_SD_IRQ_ENABLE_OFFSET,
                  0U);
        s_irq_ready = false;
    }
    if (s_controller_ready) {
        bvstk_sd_controller_shutdown(&s_controller);
    }
    if (s_mutex_ready) {
        bvstk_freertos_mutex_destroy(&s_mutex);
    }
    if (s_completion != NULL) {
        vSemaphoreDelete(s_completion);
    }
    memset(&s_controller, 0, sizeof(s_controller));
    memset(&s_mutex, 0, sizeof(s_mutex));
    s_controller_ready = false;
    s_mutex_ready = false;
    s_card_ready = false;
    s_completion = NULL;
    s_sector_count = 0U;
    s_sector_count_ready = false;
#endif
}

int bvstk_sd_pl_is_ready(void)
{
    return s_card_ready ? 1 : 0;
}

bvstk_status_t bvstk_sd_pl_get_sector_count(uint32_t *sector_count)
{
#if !BVSTK_PL_SD_CAN_BLOCK_IO
    (void)sector_count;
    return BVSTK_ERR_UNSUPPORTED;
#else
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
#endif
}

bvstk_status_t bvstk_sd_pl_read(uint32_t first_sector,
                                uint8_t *buffer,
                                size_t sector_count)
{
#if !BVSTK_PL_SD_CAN_BLOCK_IO
    (void)first_sector;
    (void)buffer;
    (void)sector_count;
    return BVSTK_ERR_UNSUPPORTED;
#else
    if (!s_card_ready) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_sd_controller_read(&s_controller,
                                    first_sector,
                                    buffer,
                                    sector_count,
                                    BVSTK_SD_PL_TRANSFER_TIMEOUT_MS);
#endif
}

bvstk_status_t bvstk_sd_pl_write(uint32_t first_sector,
                                 const uint8_t *buffer,
                                 size_t sector_count)
{
#if !BVSTK_PL_SD_CAN_BLOCK_IO
    (void)first_sector;
    (void)buffer;
    (void)sector_count;
    return BVSTK_ERR_UNSUPPORTED;
#else
    if (!s_card_ready) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_sd_controller_write(&s_controller,
                                     first_sector,
                                     buffer,
                                     sector_count,
                                     BVSTK_SD_PL_TRANSFER_TIMEOUT_MS);
#endif
}
