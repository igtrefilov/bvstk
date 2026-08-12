#include "drivers/pl/i2c/bvstk_i2c_core.h"

#include <string.h>

#include "hardware/pl/i2c/bvstk_i2c_regs.h"

enum {
    BVSTK_I2C_DEFAULT_TIMEOUT_MS = 100,
    BVSTK_I2C_RECOVERY_DELAY_MS = 10,
    BVSTK_I2C_RECOVERY_ATTEMPTS = 3,
    BVSTK_I2C_MAX_TRANSFER_BYTES = 0x1FFFFF
};

static bvstk_status_t read32(const bvstk_mmio_region_t *region,
                             size_t offset,
                             uint32_t *value)
{
    return bvstk_mmio_read32(region, offset, value) == 0
               ? BVSTK_OK
               : BVSTK_ERR_IO;
}

static bvstk_status_t write32(const bvstk_mmio_region_t *region,
                              size_t offset,
                              uint32_t value)
{
    return bvstk_mmio_write32(region, offset, value) == 0
               ? BVSTK_OK
               : BVSTK_ERR_IO;
}

static uint32_t effective_timeout(uint32_t timeout_ms)
{
    return timeout_ms == 0U ? BVSTK_I2C_DEFAULT_TIMEOUT_MS : timeout_ms;
}

static bvstk_status_t wait_ready(bvstk_i2c_core_t *core, uint32_t timeout_ms)
{
    uint64_t start;
    uint32_t csr = 0U;

    if (core == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    timeout_ms = effective_timeout(timeout_ms);
    start = bvstk_clock_now_ms(&core->clock);

    for (;;) {
        bvstk_status_t status = read32(&core->master,
                                       BVSTK_I2C_MSTR_CSR_OFFSET,
                                       &csr);
        if (status != BVSTK_OK) {
            return status;
        }
        /* The PL core exposes bit 0 as the busy/core_state bit. */
        if ((csr & UINT32_C(1)) == 0U) {
            return BVSTK_OK;
        }
        if (bvstk_clock_now_ms(&core->clock) - start >= timeout_ms) {
            return BVSTK_ERR_TIMEOUT;
        }
        bvstk_clock_sleep_ms(&core->clock, 1U);
    }
}

static void recover(bvstk_i2c_core_t *core)
{
    if (core == NULL) {
        return;
    }
    (void)write32(&core->master,
                  BVSTK_I2C_MSTR_CSR_OFFSET,
                  BVSTK_I2C_MSTR_SOFT_RESET_BIT);
    bvstk_clock_sleep_ms(&core->clock, BVSTK_I2C_RECOVERY_DELAY_MS);
}

static uint32_t make_header(uint8_t addr_7b,
                            uint8_t read,
                            uint8_t restart,
                            size_t byte_count)
{
    return (((uint32_t)byte_count & UINT32_C(0x1FFFFF)) << 11) |
           ((uint32_t)(restart != 0U) << 8) |
           ((uint32_t)(read != 0U) << 7) |
           ((uint32_t)addr_7b & UINT32_C(0x7F));
}

static bvstk_status_t write_fifo_bytes(bvstk_i2c_core_t *core,
                                       const uint8_t *payload,
                                       size_t payload_size)
{
    size_t offset = 0U;

    while (offset + sizeof(uint32_t) <= payload_size) {
        uint32_t word = (uint32_t)payload[offset] |
                        ((uint32_t)payload[offset + 1U] << 8) |
                        ((uint32_t)payload[offset + 2U] << 16) |
                        ((uint32_t)payload[offset + 3U] << 24);
        bvstk_status_t status = write32(&core->master,
                                        BVSTK_I2C_MSTR_TX_FIFO_OFFSET,
                                        word);
        if (status != BVSTK_OK) {
            return status;
        }
        offset += sizeof(uint32_t);
    }

    if (offset < payload_size) {
        uint32_t word = 0U;
        size_t byte;
        for (byte = 0U; offset + byte < payload_size; ++byte) {
            word |= (uint32_t)payload[offset + byte] << (8U * byte);
        }
        return write32(&core->master,
                       BVSTK_I2C_MSTR_TX_FIFO_OFFSET,
                       word);
    }
    return BVSTK_OK;
}

static bvstk_status_t transfer_locked(bvstk_i2c_core_t *core,
                                       uint8_t addr_7b,
                                       uint8_t read,
                                       uint8_t restart,
                                       const uint8_t *payload,
                                       size_t payload_size,
                                       uint32_t timeout_ms)
{
    unsigned attempt;
    bvstk_status_t status = BVSTK_ERR_TIMEOUT;

    if (payload_size > BVSTK_I2C_MAX_TRANSFER_BYTES ||
        (payload_size != 0U && payload == NULL)) {
        return BVSTK_ERR_RANGE;
    }

    for (attempt = 0U; attempt < BVSTK_I2C_RECOVERY_ATTEMPTS; ++attempt) {
        status = wait_ready(core, timeout_ms);
        if (status != BVSTK_OK) {
            recover(core);
            continue;
        }

        status = write32(&core->master,
                         BVSTK_I2C_MSTR_TX_FIFO_OFFSET,
                         make_header(addr_7b, read, restart, payload_size));
        if (status == BVSTK_OK) {
            status = write_fifo_bytes(core, payload, payload_size);
        }
        if (status == BVSTK_OK) {
            status = write32(&core->master,
                             BVSTK_I2C_MSTR_CSR_OFFSET,
                             BVSTK_I2C_MSTR_START_BIT);
        }
        if (status == BVSTK_OK) {
            status = wait_ready(core, timeout_ms);
        }
        if (status == BVSTK_OK) {
            return BVSTK_OK;
        }
        recover(core);
    }
    return status;
}

static bvstk_status_t read_reg_locked(bvstk_i2c_core_t *core,
                                       uint8_t addr_7b,
                                       uint8_t reg,
                                       uint32_t timeout_ms)
{
    unsigned attempt;
    bvstk_status_t status = BVSTK_ERR_TIMEOUT;
    uint32_t first_header = make_header(addr_7b, 0U, 1U, 1U);
    uint32_t second_header = make_header(addr_7b, 1U, 0U, 1U);

    /*
     * A register read is one PL transaction.  Both phase headers must be
     * queued before START; issuing the pointer phase and read phase through
     * transfer_locked() separately would release the repeated-start FIFO
     * sequence and is not equivalent on the hardware core.
     */
    for (attempt = 0U; attempt < BVSTK_I2C_RECOVERY_ATTEMPTS; ++attempt) {
        status = wait_ready(core, timeout_ms);
        if (status != BVSTK_OK) {
            recover(core);
            continue;
        }

        status = write32(&core->master,
                         BVSTK_I2C_MSTR_TX_FIFO_OFFSET,
                         first_header);
        if (status == BVSTK_OK) {
            status = write32(&core->master,
                             BVSTK_I2C_MSTR_TX_FIFO_OFFSET,
                             (uint32_t)reg);
        }
        if (status == BVSTK_OK) {
            status = write32(&core->master,
                             BVSTK_I2C_MSTR_TX_FIFO_OFFSET,
                             second_header);
        }
        if (status == BVSTK_OK) {
            status = write32(&core->master,
                             BVSTK_I2C_MSTR_CSR_OFFSET,
                             BVSTK_I2C_MSTR_START_BIT);
        }
        if (status == BVSTK_OK) {
            status = wait_ready(core, timeout_ms);
        }
        if (status == BVSTK_OK) {
            return BVSTK_OK;
        }
        recover(core);
    }
    return status;
}

bvstk_status_t bvstk_i2c_core_init(bvstk_i2c_core_t *core,
                                   const bvstk_clock_t *clock,
                                   const bvstk_mutex_t *mutex)
{
    if (core == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(core, 0, sizeof(*core));
    if (clock != NULL) {
        core->clock = *clock;
    }
    if (mutex != NULL) {
        core->mutex = *mutex;
    }

    if (bvstk_mmio_region_open(&core->master,
                               BVSTK_I2C_MASTER_BASE,
                               BVSTK_I2C_MASTER_SIZE) != 0) {
        return BVSTK_ERR_IO;
    }
    if (bvstk_mmio_region_open(&core->bram,
                               BVSTK_I2C_BRAM_BASE,
                               BVSTK_I2C_BRAM_SIZE) != 0) {
        bvstk_mmio_region_close(&core->master);
        return BVSTK_ERR_IO;
    }
    core->initialized = 1U;
    return BVSTK_OK;
}

void bvstk_i2c_core_shutdown(bvstk_i2c_core_t *core)
{
    if (core == NULL) {
        return;
    }
    if (core->initialized != 0U) {
        bvstk_mmio_region_close(&core->bram);
        bvstk_mmio_region_close(&core->master);
    }
    memset(core, 0, sizeof(*core));
}

bvstk_status_t bvstk_i2c_core_write(bvstk_i2c_core_t *core,
                                    uint8_t addr_7b,
                                    const uint8_t *payload,
                                    size_t payload_size,
                                    uint32_t timeout_ms)
{
    bvstk_status_t status;

    if (core == NULL || core->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    status = bvstk_mutex_lock(&core->mutex, timeout_ms);
    if (status != BVSTK_OK) {
        return status;
    }
    status = transfer_locked(core, (uint8_t)(addr_7b & 0x7FU), 0U, 0U,
                             payload, payload_size, timeout_ms);
    bvstk_mutex_unlock(&core->mutex);
    return status;
}

bvstk_status_t bvstk_i2c_core_read_reg(bvstk_i2c_core_t *core,
                                       uint8_t addr_7b,
                                       uint8_t reg,
                                       uint8_t *value,
                                       uint32_t timeout_ms)
{
    bvstk_status_t status;
    uint32_t word = 0U;

    if (core == NULL || core->initialized == 0U || value == NULL) {
        return value == NULL ? BVSTK_ERR_MALFORMED : BVSTK_ERR_NOT_READY;
    }
    status = bvstk_mutex_lock(&core->mutex, timeout_ms);
    if (status != BVSTK_OK) {
        return status;
    }
    status = read_reg_locked(core, (uint8_t)(addr_7b & 0x7FU), reg, timeout_ms);
    if (status == BVSTK_OK) {
        status = read32(&core->bram,
                        BVSTK_I2C_BRAM_MASTER_OFFSET + sizeof(uint32_t),
                        &word);
        if (status == BVSTK_OK) {
            *value = (uint8_t)(word & UINT32_C(0xFF));
        }
    }
    bvstk_mutex_unlock(&core->mutex);
    return status;
}

bvstk_status_t bvstk_i2c_core_write_reg(bvstk_i2c_core_t *core,
                                        uint8_t addr_7b,
                                        uint8_t reg,
                                        uint8_t value,
                                        uint32_t timeout_ms)
{
    uint8_t payload[2] = {reg, value};
    return bvstk_i2c_core_write(core, addr_7b, payload, sizeof(payload), timeout_ms);
}
