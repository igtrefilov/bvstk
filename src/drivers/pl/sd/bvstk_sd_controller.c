#include "drivers/pl/sd/bvstk_sd_controller.h"

#include <string.h>

#include "hardware/pl/sd/bvstk_sd_regs.h"

enum {
    BVSTK_SD_DEFAULT_TIMEOUT_MS = 5000U
};

static bvstk_status_t read32(const bvstk_sd_controller_t *controller,
                             size_t offset,
                             uint32_t *value)
{
    return bvstk_mmio_read32(&controller->registers, offset, value) == 0
               ? BVSTK_OK
               : BVSTK_ERR_IO;
}

static bvstk_status_t write32(const bvstk_sd_controller_t *controller,
                              size_t offset,
                              uint32_t value)
{
    return bvstk_mmio_write32(&controller->registers, offset, value) == 0
               ? BVSTK_OK
               : BVSTK_ERR_IO;
}

static uint32_t effective_timeout(const bvstk_sd_controller_t *controller,
                                  uint32_t timeout_ms)
{
    if (timeout_ms != 0U) {
        return timeout_ms;
    }
    if (controller->config.timeout_ms != 0U) {
        return controller->config.timeout_ms;
    }
    return BVSTK_SD_DEFAULT_TIMEOUT_MS;
}

static bvstk_status_t normalize_config(
    const bvstk_sd_controller_config_t *input,
    bvstk_sd_controller_config_t *output)
{
    if (output == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    if (input != NULL) {
        *output = *input;
    } else {
        output->timeout_ms = BVSTK_SD_DEFAULT_TIMEOUT_MS;
        output->clock_div = BVSTK_SD_CLOCK_DIV_DEFAULT;
    }
    if (output->timeout_ms == 0U) {
        output->timeout_ms = BVSTK_SD_DEFAULT_TIMEOUT_MS;
    }
    if (output->clock_div == 0U) {
        output->clock_div = BVSTK_SD_CLOCK_DIV_DEFAULT;
    }
    return BVSTK_OK;
}

static bvstk_status_t lock_controller(bvstk_sd_controller_t *controller,
                                       uint32_t timeout_ms)
{
    return bvstk_mutex_lock(&controller->mutex, effective_timeout(controller,
                                                                    timeout_ms));
}

static bvstk_status_t read_status(
    const bvstk_sd_controller_t *controller,
    bvstk_sd_controller_status_t *status)
{
    uint32_t value;
    bvstk_status_t result;

    if (status == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    result = read32(controller, BVSTK_SD_STATUS_OFFSET, &value);
    if (result != BVSTK_OK) {
        return result;
    }
    status->busy = (value & BVSTK_SD_STATUS_BUSY) != 0U;
    status->done = (value & BVSTK_SD_STATUS_DONE) != 0U;
    status->error = (value & BVSTK_SD_STATUS_ERROR) != 0U;
    status->initialized = (value & BVSTK_SD_STATUS_INITIALIZED) != 0U;
    status->high_capacity = (value & BVSTK_SD_STATUS_HIGH_CAPACITY) != 0U;
    status->error_code = (uint8_t)((value & BVSTK_SD_STATUS_ERROR_MASK) >>
                                   BVSTK_SD_STATUS_ERROR_SHIFT);
    status->last_r1 = (uint8_t)((value & BVSTK_SD_STATUS_LAST_R1_MASK) >>
                                BVSTK_SD_STATUS_LAST_R1_SHIFT);
    status->current_cmd = (uint8_t)((value & BVSTK_SD_STATUS_CURRENT_CMD_MASK) >>
                                    BVSTK_SD_STATUS_CURRENT_CMD_SHIFT);
    return BVSTK_OK;
}

static bvstk_status_t wait_for_done(bvstk_sd_controller_t *controller,
                                    uint32_t timeout_ms)
{
    uint64_t started = bvstk_clock_now_ms(&controller->clock);
    uint32_t effective = effective_timeout(controller, timeout_ms);

    for (;;) {
        bvstk_sd_controller_status_t status;
        bvstk_status_t result = read_status(controller, &status);

        if (result != BVSTK_OK) {
            return result;
        }
        if (!status.busy) {
            if (status.error) {
                return BVSTK_ERR_IO;
            }
            return status.done ? BVSTK_OK : BVSTK_ERR_IO;
        }
        if (bvstk_clock_now_ms(&controller->clock) - started >= effective) {
            return BVSTK_ERR_TIMEOUT;
        }
        bvstk_clock_sleep_ms(&controller->clock, 1U);
    }
}

static bvstk_status_t clear_result(bvstk_sd_controller_t *controller)
{
    return write32(controller,
                   BVSTK_SD_CONTROL_OFFSET,
                   BVSTK_SD_CONTROL_CLEAR);
}

static bvstk_status_t copy_buffer_from_controller(
    const bvstk_sd_controller_t *controller,
    uint8_t *buffer)
{
    size_t word;

    if (write32(controller, BVSTK_SD_BUFFER_INDEX_OFFSET, 0U) != BVSTK_OK) {
        return BVSTK_ERR_IO;
    }
    for (word = 0U; word < BVSTK_SD_BUFFER_WORDS; ++word) {
        uint32_t value;
        bvstk_status_t result = read32(controller,
                                       BVSTK_SD_DATA_OFFSET,
                                       &value);
        if (result != BVSTK_OK) {
            return result;
        }
        memcpy(buffer + word * sizeof(value), &value, sizeof(value));
    }
    return BVSTK_OK;
}

static bvstk_status_t copy_buffer_to_controller(
    const bvstk_sd_controller_t *controller,
    const uint8_t *buffer)
{
    size_t word;

    if (write32(controller, BVSTK_SD_BUFFER_INDEX_OFFSET, 0U) != BVSTK_OK) {
        return BVSTK_ERR_IO;
    }
    for (word = 0U; word < BVSTK_SD_BUFFER_WORDS; ++word) {
        uint32_t value;
        memcpy(&value, buffer + word * sizeof(value), sizeof(value));
        if (write32(controller, BVSTK_SD_DATA_OFFSET, value) != BVSTK_OK) {
            return BVSTK_ERR_IO;
        }
    }
    return BVSTK_OK;
}

static bvstk_status_t transfer_one_sector(
    bvstk_sd_controller_t *controller,
    uint32_t sector,
    uint8_t *read_buffer,
    const uint8_t *write_buffer,
    bool write,
    uint32_t timeout_ms)
{
    bvstk_status_t result;

    result = write32(controller, BVSTK_SD_BLOCK_OFFSET, sector);
    if (result != BVSTK_OK) {
        return result;
    }
    if (write) {
        result = copy_buffer_to_controller(controller, write_buffer);
    }
    if (result == BVSTK_OK) {
        result = clear_result(controller);
    }
    if (result == BVSTK_OK) {
        result = write32(controller,
                         BVSTK_SD_CONTROL_OFFSET,
                         write ? BVSTK_SD_CONTROL_WRITE : BVSTK_SD_CONTROL_READ);
    }
    if (result == BVSTK_OK) {
        result = wait_for_done(controller, timeout_ms);
    }
    if (result == BVSTK_OK && !write) {
        result = copy_buffer_from_controller(controller, read_buffer);
    }
    return result;
}

bvstk_status_t bvstk_sd_controller_init(
    bvstk_sd_controller_t *controller,
    const bvstk_sd_controller_config_t *config,
    const bvstk_clock_t *clock,
    const bvstk_mutex_t *mutex)
{
    bvstk_sd_controller_config_t normalized;

    if (controller == NULL || normalize_config(config, &normalized) != BVSTK_OK) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(controller, 0, sizeof(*controller));
    controller->config = normalized;
    if (clock != NULL) {
        controller->clock = *clock;
    }
    if (mutex != NULL) {
        controller->mutex = *mutex;
    }
    if (bvstk_mmio_region_open(&controller->registers,
                               BVSTK_SD_CONTROLLER_BASE,
                               BVSTK_SD_CONTROLLER_SIZE) != 0) {
        return BVSTK_ERR_IO;
    }
    controller->initialized = true;
    if (write32(controller,
                BVSTK_SD_CLOCK_DIV_OFFSET,
                controller->config.clock_div) != BVSTK_OK) {
        bvstk_sd_controller_shutdown(controller);
        return BVSTK_ERR_IO;
    }
    return BVSTK_OK;
}

void bvstk_sd_controller_shutdown(bvstk_sd_controller_t *controller)
{
    if (controller == NULL) {
        return;
    }
    if (controller->initialized) {
        bvstk_mmio_region_close(&controller->registers);
    }
    memset(controller, 0, sizeof(*controller));
}

bvstk_status_t bvstk_sd_controller_get_status(
    const bvstk_sd_controller_t *controller,
    bvstk_sd_controller_status_t *status)
{
    if (controller == NULL || !controller->initialized) {
        return BVSTK_ERR_NOT_READY;
    }
    return read_status(controller, status);
}

bvstk_status_t bvstk_sd_controller_get_debug(
    const bvstk_sd_controller_t *controller,
    bvstk_sd_controller_debug_t *debug)
{
    size_t index;

    if (controller == NULL || !controller->initialized || debug == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    memset(debug, 0, sizeof(*debug));
    {
        struct debug_register {
            size_t offset;
            uint32_t *value;
        } registers[] = {
            {BVSTK_SD_DEBUG_STATE_OFFSET, &debug->state},
            {BVSTK_SD_DEBUG_COUNTERS_OFFSET, &debug->counters},
            {BVSTK_SD_DEBUG_LAST_CMD_HI_OFFSET, &debug->last_cmd_hi},
            {BVSTK_SD_DEBUG_LAST_CMD_LO_OFFSET, &debug->last_cmd_lo},
            {BVSTK_SD_DEBUG_RESPONSE_OFFSET, &debug->response},
            {BVSTK_SD_DEBUG_ACMD41_OFFSET, &debug->acmd41},
            {BVSTK_SD_DEBUG_CMD55_OFFSET, &debug->cmd55},
            {BVSTK_SD_DEBUG_PINS_OFFSET, &debug->pins},
            {BVSTK_SD_DEBUG_CMD55_HI_OFFSET, &debug->cmd55_hi},
            {BVSTK_SD_DEBUG_CMD55_LO_OFFSET, &debug->cmd55_lo},
            {BVSTK_SD_DEBUG_CMD41_HI_OFFSET, &debug->cmd41_hi},
            {BVSTK_SD_DEBUG_CMD41_LO_OFFSET, &debug->cmd41_lo},
            {BVSTK_SD_DEBUG_LAST_BYTE_OFFSET, &debug->last_byte},
            {BVSTK_SD_DEBUG_DIAG_OFFSET, &debug->diag}
        };
        for (index = 0U; index < sizeof(registers) / sizeof(registers[0]); ++index) {
            if (read32(controller,
                       registers[index].offset,
                       registers[index].value) != BVSTK_OK) {
                return BVSTK_ERR_IO;
            }
        }
    }
    for (index = 0U; index < BVSTK_SD_DEBUG_TRACE_LENGTH; ++index) {
        if (write32(controller,
                    BVSTK_SD_DEBUG_TRACE_INDEX_OFFSET,
                    (uint32_t)index) != BVSTK_OK ||
            read32(controller,
                   BVSTK_SD_DEBUG_TRACE_DATA_OFFSET,
                   &debug->trace[index]) != BVSTK_OK) {
            return BVSTK_ERR_IO;
        }
    }
    return BVSTK_OK;
}

bvstk_status_t bvstk_sd_controller_initialize_card(
    bvstk_sd_controller_t *controller,
    uint32_t timeout_ms)
{
    bvstk_status_t result;

    if (controller == NULL || !controller->initialized) {
        return BVSTK_ERR_NOT_READY;
    }
    result = lock_controller(controller, timeout_ms);
    if (result != BVSTK_OK) {
        return result;
    }
    result = clear_result(controller);
    if (result == BVSTK_OK) {
        result = write32(controller,
                         BVSTK_SD_CONTROL_OFFSET,
                         BVSTK_SD_CONTROL_INIT);
    }
    if (result == BVSTK_OK) {
        result = wait_for_done(controller, timeout_ms);
    }
    bvstk_mutex_unlock(&controller->mutex);
    return result;
}

bvstk_status_t bvstk_sd_controller_read(
    bvstk_sd_controller_t *controller,
    uint32_t first_sector,
    uint8_t *buffer,
    size_t sector_count,
    uint32_t timeout_ms)
{
    size_t sector;
    bvstk_status_t result;

    if (controller == NULL || !controller->initialized) {
        return BVSTK_ERR_NOT_READY;
    }
    if (buffer == NULL || sector_count == 0U ||
        sector_count - 1U > (size_t)UINT32_MAX - first_sector) {
        return BVSTK_ERR_RANGE;
    }
    result = lock_controller(controller, timeout_ms);
    if (result != BVSTK_OK) {
        return result;
    }
    {
        bvstk_sd_controller_status_t status;
        result = read_status(controller, &status);
        if (result == BVSTK_OK && !status.initialized) {
            result = BVSTK_ERR_NOT_READY;
        }
    }
    for (sector = 0U; result == BVSTK_OK && sector < sector_count; ++sector) {
        result = transfer_one_sector(controller,
                                     first_sector + (uint32_t)sector,
                                     buffer + sector * BVSTK_SD_BLOCK_SIZE,
                                     NULL,
                                     false,
                                     timeout_ms);
    }
    bvstk_mutex_unlock(&controller->mutex);
    return result;
}

bvstk_status_t bvstk_sd_controller_write(
    bvstk_sd_controller_t *controller,
    uint32_t first_sector,
    const uint8_t *buffer,
    size_t sector_count,
    uint32_t timeout_ms)
{
    size_t sector;
    bvstk_status_t result;

    if (controller == NULL || !controller->initialized) {
        return BVSTK_ERR_NOT_READY;
    }
    if (buffer == NULL || sector_count == 0U ||
        sector_count - 1U > (size_t)UINT32_MAX - first_sector) {
        return BVSTK_ERR_RANGE;
    }
    result = lock_controller(controller, timeout_ms);
    if (result != BVSTK_OK) {
        return result;
    }
    {
        bvstk_sd_controller_status_t status;
        result = read_status(controller, &status);
        if (result == BVSTK_OK && !status.initialized) {
            result = BVSTK_ERR_NOT_READY;
        }
    }
    for (sector = 0U; result == BVSTK_OK && sector < sector_count; ++sector) {
        result = transfer_one_sector(controller,
                                     first_sector + (uint32_t)sector,
                                     NULL,
                                     buffer + sector * BVSTK_SD_BLOCK_SIZE,
                                     true,
                                     timeout_ms);
    }
    bvstk_mutex_unlock(&controller->mutex);
    return result;
}
