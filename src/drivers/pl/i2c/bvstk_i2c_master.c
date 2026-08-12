#include "drivers/pl/i2c/bvstk_i2c_master.h"

#include <string.h>

#include "hardware/pl/i2c/bvstk_i2c_regs.h"

enum {
    BVSTK_I2C_DEFAULT_TIMEOUT_MS = 100,
    BVSTK_I2C_RECOVERY_DELAY_MS = 10,
    BVSTK_I2C_RECOVERY_ATTEMPTS = 3
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

static bvstk_status_t wait_ready(bvstk_i2c_master_hw_t *hardware,
                                 uint32_t timeout_ms)
{
    uint64_t start;
    uint32_t csr = 0U;

    if (hardware == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    timeout_ms = effective_timeout(timeout_ms);
    start = bvstk_clock_now_ms(&hardware->clock);
    for (;;) {
        bvstk_status_t status = read32(&hardware->master,
                                       BVSTK_I2C_MSTR_CSR_OFFSET,
                                       &csr);
        if (status != BVSTK_OK) {
            return status;
        }
        if ((csr & UINT32_C(1)) == 0U) {
            return BVSTK_OK;
        }
        if (bvstk_clock_now_ms(&hardware->clock) - start >= timeout_ms) {
            return BVSTK_ERR_TIMEOUT;
        }
        bvstk_clock_sleep_ms(&hardware->clock, 1U);
    }
}

static void recover(bvstk_i2c_master_hw_t *hardware)
{
    if (hardware == NULL) {
        return;
    }
    (void)write32(&hardware->master,
                  BVSTK_I2C_MSTR_CSR_OFFSET,
                  BVSTK_I2C_MSTR_SOFT_RESET_BIT);
    bvstk_clock_sleep_ms(&hardware->clock, BVSTK_I2C_RECOVERY_DELAY_MS);
}

static bvstk_status_t write_fifo_bytes(bvstk_i2c_master_hw_t *hardware,
                                       const uint8_t *data,
                                       size_t size)
{
    size_t offset = 0U;

    while (offset + sizeof(uint32_t) <= size) {
        uint32_t word = (uint32_t)data[offset] |
                        ((uint32_t)data[offset + 1U] << 8) |
                        ((uint32_t)data[offset + 2U] << 16) |
                        ((uint32_t)data[offset + 3U] << 24);
        bvstk_status_t status = write32(&hardware->master,
                                        BVSTK_I2C_MSTR_TX_FIFO_OFFSET,
                                        word);
        if (status != BVSTK_OK) {
            return status;
        }
        offset += sizeof(uint32_t);
    }
    if (offset < size) {
        uint32_t word = 0U;
        size_t byte;
        for (byte = 0U; offset + byte < size; ++byte) {
            word |= (uint32_t)data[offset + byte] << (8U * byte);
        }
        return write32(&hardware->master,
                       BVSTK_I2C_MSTR_TX_FIFO_OFFSET,
                       word);
    }
    return BVSTK_OK;
}

static bvstk_status_t transfer_locked(bvstk_i2c_master_hw_t *hardware,
                                      uint8_t addr_7b,
                                      const uint8_t *write_data,
                                      size_t write_size,
                                      uint8_t *read_data,
                                      size_t read_size,
                                      uint32_t timeout_ms)
{
    unsigned attempt;
    bvstk_status_t status;

    if (write_size == 0U && read_size == 0U) {
        return BVSTK_ERR_MALFORMED;
    }
    if ((write_size != 0U && write_data == NULL) ||
        (read_size != 0U && read_data == NULL) ||
        write_size > UINT32_C(0x1FFFFF) ||
        read_size > UINT32_C(0x1FFFFF) ||
        read_size > BVSTK_I2C_MASTER_RESULT_MAX_BYTES) {
        return BVSTK_ERR_RANGE;
    }

    for (attempt = 0U; attempt < BVSTK_I2C_RECOVERY_ATTEMPTS; ++attempt) {
        status = wait_ready(hardware, timeout_ms);
        if (status != BVSTK_OK) {
            recover(hardware);
            continue;
        }

        status = BVSTK_OK;
        if (write_size != 0U) {
            status = write32(&hardware->master,
                             BVSTK_I2C_MSTR_TX_FIFO_OFFSET,
                             make_header(addr_7b,
                                         0U,
                                         read_size != 0U ? 1U : 0U,
                                         write_size));
            if (status == BVSTK_OK) {
                status = write_fifo_bytes(hardware, write_data, write_size);
            }
        }
        if (status == BVSTK_OK && read_size != 0U) {
            status = write32(&hardware->master,
                             BVSTK_I2C_MSTR_TX_FIFO_OFFSET,
                             make_header(addr_7b, 1U, 0U, read_size));
        }
        if (status == BVSTK_OK) {
            status = write32(&hardware->master,
                             BVSTK_I2C_MSTR_CSR_OFFSET,
                             BVSTK_I2C_MSTR_START_BIT);
        }
        if (status == BVSTK_OK) {
            status = wait_ready(hardware, timeout_ms);
        }
        if (status == BVSTK_OK && read_size != 0U) {
            size_t offset = 0U;
            while (offset < read_size) {
                uint32_t word = 0U;
                size_t byte;
                size_t count = read_size - offset;
                if (count > sizeof(word)) {
                    count = sizeof(word);
                }
                status = read32(&hardware->bram,
                                BVSTK_I2C_BRAM_MASTER_OFFSET +
                                    sizeof(uint32_t) + offset,
                                &word);
                if (status != BVSTK_OK) {
                    break;
                }
                for (byte = 0U; byte < count; ++byte) {
                    read_data[offset + byte] = (uint8_t)(word >> (8U * byte));
                }
                offset += count;
            }
        }
        if (status == BVSTK_OK) {
            return BVSTK_OK;
        }
        recover(hardware);
    }
    return status;
}

bvstk_status_t bvstk_i2c_master_hw_init(bvstk_i2c_master_hw_t *hardware,
                                         const bvstk_clock_t *clock,
                                         const bvstk_mutex_t *mutex)
{
    if (hardware == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(hardware, 0, sizeof(*hardware));
    if (clock != NULL) {
        hardware->clock = *clock;
    }
    if (mutex != NULL) {
        hardware->mutex = *mutex;
    }
    if (bvstk_mmio_region_open(&hardware->master,
                               BVSTK_I2C_MASTER_BASE,
                               BVSTK_I2C_MASTER_SIZE) != 0) {
        return BVSTK_ERR_IO;
    }
    if (bvstk_mmio_region_open(&hardware->bram,
                               BVSTK_I2C_BRAM_BASE,
                               BVSTK_I2C_BRAM_SIZE) != 0) {
        bvstk_mmio_region_close(&hardware->master);
        return BVSTK_ERR_IO;
    }
    hardware->initialized = 1U;
    return BVSTK_OK;
}

void bvstk_i2c_master_hw_shutdown(bvstk_i2c_master_hw_t *hardware)
{
    if (hardware == NULL) {
        return;
    }
    if (hardware->initialized != 0U) {
        bvstk_mmio_region_close(&hardware->bram);
        bvstk_mmio_region_close(&hardware->master);
    }
    memset(hardware, 0, sizeof(*hardware));
}

bvstk_status_t bvstk_i2c_master_hw_transfer(
    bvstk_i2c_master_hw_t *hardware,
    uint8_t addr_7b,
    const uint8_t *write_data,
    size_t write_size,
    uint8_t *read_data,
    size_t read_size,
    uint32_t timeout_ms)
{
    bvstk_status_t status;

    if (hardware == NULL || hardware->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    status = bvstk_mutex_lock(&hardware->mutex, timeout_ms);
    if (status != BVSTK_OK) {
        return status;
    }
    status = transfer_locked(hardware,
                             (uint8_t)(addr_7b & UINT8_C(0x7F)),
                             write_data,
                             write_size,
                             read_data,
                             read_size,
                             timeout_ms);
    bvstk_mutex_unlock(&hardware->mutex);
    return status;
}

bvstk_status_t bvstk_i2c_master_hw_read_reg(bvstk_i2c_master_hw_t *hardware,
                                            uint8_t addr_7b,
                                            uint8_t reg,
                                            uint8_t *value,
                                            uint32_t timeout_ms)
{
    return bvstk_i2c_master_hw_transfer(hardware,
                                        addr_7b,
                                        &reg,
                                        1U,
                                        value,
                                        1U,
                                        timeout_ms);
}

bvstk_status_t bvstk_i2c_master_hw_write_reg(bvstk_i2c_master_hw_t *hardware,
                                             uint8_t addr_7b,
                                             uint8_t reg,
                                             uint8_t value,
                                             uint32_t timeout_ms)
{
    const uint8_t payload[2] = {reg, value};
    return bvstk_i2c_master_hw_transfer(hardware,
                                        addr_7b,
                                        payload,
                                        sizeof(payload),
                                        NULL,
                                        0U,
                                        timeout_ms);
}
