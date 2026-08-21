#include "drivers/pl/sd/bvstk_sd_init.h"

#include <string.h>

enum {
    BVSTK_SD_INIT_DEFAULT_TIMEOUT_MS = 20000U,
    BVSTK_SD_INIT_RESET_DELAY_MS = 1U,
    BVSTK_SD_INIT_POLL_DELAY_MS = 1U
};

static bvstk_status_t read32(const bvstk_mmio_region_t *region,
                             uint32_t offset,
                             uint32_t *value)
{
    return bvstk_mmio_read32(region, offset, value) == 0
               ? BVSTK_OK
               : BVSTK_ERR_IO;
}

static bvstk_status_t write32(const bvstk_mmio_region_t *region,
                              uint32_t offset,
                              uint32_t value)
{
    return bvstk_mmio_write32(region, offset, value) == 0
               ? BVSTK_OK
               : BVSTK_ERR_IO;
}

static uint32_t effective_timeout(uint32_t timeout_ms)
{
    return timeout_ms == 0U ? BVSTK_SD_INIT_DEFAULT_TIMEOUT_MS : timeout_ms;
}

static void clear_irq(const bvstk_sd_init_t *driver)
{
    (void)write32(&driver->control, BVSTK_SD_INIT_IRQ_OFFSET, 1U);
    (void)write32(&driver->control, BVSTK_SD_INIT_IRQ_OFFSET, 0U);
}

static void clear_responses(const bvstk_sd_init_t *driver)
{
    static const uint32_t offsets[BVSTK_SD_INIT_RESPONSE_COUNT] = {
        BVSTK_SD_INIT_RESP_CMD0,
        BVSTK_SD_INIT_RESP_CMD8_FIRST,
        BVSTK_SD_INIT_RESP_CMD8_SECOND,
        BVSTK_SD_INIT_RESP_ACMD41,
        BVSTK_SD_INIT_RESP_CMD55,
        BVSTK_SD_INIT_RESP_CMD58_FIRST,
        BVSTK_SD_INIT_RESP_CMD58_SECOND
    };

    for (size_t i = 0U; i < BVSTK_SD_INIT_RESPONSE_COUNT; ++i) {
        (void)write32(&driver->response, offsets[i], 0U);
    }
}

static void snapshot_responses(const bvstk_sd_init_t *driver,
                               bvstk_sd_init_result_t *result)
{
    static const uint32_t offsets[BVSTK_SD_INIT_RESPONSE_COUNT] = {
        BVSTK_SD_INIT_RESP_CMD0,
        BVSTK_SD_INIT_RESP_CMD8_FIRST,
        BVSTK_SD_INIT_RESP_CMD8_SECOND,
        BVSTK_SD_INIT_RESP_ACMD41,
        BVSTK_SD_INIT_RESP_CMD55,
        BVSTK_SD_INIT_RESP_CMD58_FIRST,
        BVSTK_SD_INIT_RESP_CMD58_SECOND
    };

    for (size_t i = 0U; i < BVSTK_SD_INIT_RESPONSE_COUNT; ++i) {
        (void)read32(&driver->response, offsets[i], &result->response[i]);
    }
}

bvstk_status_t bvstk_sd_init_init(bvstk_sd_init_t *driver,
                                   const bvstk_clock_t *clock,
                                   const bvstk_mutex_t *mutex)
{
    if (driver == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(driver, 0, sizeof(*driver));
    if (clock != NULL) {
        driver->clock = *clock;
    }
    if (mutex != NULL) {
        driver->mutex = *mutex;
    }
    if (bvstk_mmio_region_open(&driver->control,
                               BVSTK_SD_CONTROLLER_BASE,
                               BVSTK_SD_CONTROLLER_SIZE) != 0) {
        return BVSTK_ERR_IO;
    }
    if (bvstk_mmio_region_open(&driver->response,
                               BVSTK_SPI_BRAM_BASE,
                               BVSTK_SPI_BRAM_SIZE) != 0) {
        bvstk_mmio_region_close(&driver->control);
        return BVSTK_ERR_IO;
    }
    driver->initialized = 1U;
    return BVSTK_OK;
}

void bvstk_sd_init_shutdown(bvstk_sd_init_t *driver)
{
    if (driver == NULL) {
        return;
    }
    if (driver->initialized != 0U) {
        bvstk_mmio_region_close(&driver->response);
        bvstk_mmio_region_close(&driver->control);
    }
    memset(driver, 0, sizeof(*driver));
}

bvstk_status_t bvstk_sd_init_run(bvstk_sd_init_t *driver,
                                 uint32_t timeout_ms,
                                 bvstk_sd_init_result_t *result)
{
    bvstk_status_t status = BVSTK_OK;
    uint32_t irq = 0U;
    uint64_t started;
    uint32_t effective;

    if (driver == NULL || driver->initialized == 0U || result == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    memset(result, 0, sizeof(*result));
    effective = effective_timeout(timeout_ms);
    status = bvstk_mutex_lock(&driver->mutex, effective);
    if (status != BVSTK_OK) {
        return status;
    }

    status = write32(&driver->control,
                     BVSTK_SD_INIT_CSR_OFFSET,
                     BVSTK_SD_INIT_CSR_SOFT_RESET);
    if (status == BVSTK_OK) {
        bvstk_clock_sleep_ms(&driver->clock, BVSTK_SD_INIT_RESET_DELAY_MS);
        clear_responses(driver);
        status = write32(&driver->control,
                         BVSTK_SD_INIT_CSR_OFFSET,
                         BVSTK_SD_INIT_CSR_START |
                             BVSTK_SD_INIT_CSR_READ_ENABLE);
    }
    if (status == BVSTK_OK) {
        status = write32(&driver->control,
                         BVSTK_SD_INIT_PACKET_OFFSET,
                         BVSTK_SD_INIT_PACKET_MODE);
    }
    if (status == BVSTK_OK) {
        status = write32(&driver->control,
                         BVSTK_SD_INIT_TIMEOUT_OFFSET,
                         BVSTK_SD_INIT_TIMEOUT_TICKS);
    }
    if (status == BVSTK_OK) {
        status = write32(&driver->control,
                         BVSTK_SD_INIT_SPI_SIG_OFFSET,
                         BVSTK_SD_INIT_CLOCK_DIV);
    }
    if (status == BVSTK_OK) {
        clear_irq(driver);
        status = write32(&driver->control,
                         BVSTK_SD_INIT_CONTROL_OFFSET,
                         (BVSTK_SD_INIT_CLOCK_COUNT << 2U) | 3U);
    }

    started = bvstk_clock_now_ms(&driver->clock);
    while (status == BVSTK_OK) {
        status = read32(&driver->control, BVSTK_SD_INIT_IRQ_OFFSET, &irq);
        if (status != BVSTK_OK) {
            break;
        }
        result->irq = irq;
        result->hard_done =
            (uint8_t)((irq & BVSTK_SD_INIT_IRQ_HARD_DONE) != 0U);
        result->soft_done =
            (uint8_t)((irq & BVSTK_SD_INIT_IRQ_SOFT_DONE) != 0U);
        if (result->soft_done != 0U) {
            break;
        }
        if (bvstk_clock_now_ms(&driver->clock) - started >= effective) {
            status = BVSTK_ERR_TIMEOUT;
            break;
        }
        bvstk_clock_sleep_ms(&driver->clock, BVSTK_SD_INIT_POLL_DELAY_MS);
    }

    (void)read32(&driver->control, BVSTK_SD_INIT_CSR_OFFSET, &result->csr);
    snapshot_responses(driver, result);
    if (status == BVSTK_OK && result->soft_done == 0U) {
        status = BVSTK_ERR_IO;
    }
    clear_irq(driver);
    bvstk_mutex_unlock(&driver->mutex);
    return status;
}
