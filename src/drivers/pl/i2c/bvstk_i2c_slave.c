#include "drivers/pl/i2c/bvstk_i2c_slave.h"

#include <string.h>

#include "hardware/pl/i2c/bvstk_i2c_regs.h"

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

bvstk_status_t bvstk_i2c_slave_hw_init(bvstk_i2c_slave_hw_t *hardware)
{
    if (hardware == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(hardware, 0, sizeof(*hardware));
    if (bvstk_mmio_region_open(&hardware->slave,
                               BVSTK_I2C_SLAVE_BASE,
                               BVSTK_I2C_SLAVE_SIZE) != 0) {
        return BVSTK_ERR_IO;
    }
    if (bvstk_mmio_region_open(&hardware->bram,
                               BVSTK_I2C_BRAM_BASE,
                               BVSTK_I2C_BRAM_SIZE) != 0) {
        bvstk_mmio_region_close(&hardware->slave);
        return BVSTK_ERR_IO;
    }
    hardware->initialized = 1U;
    return BVSTK_OK;
}

void bvstk_i2c_slave_hw_shutdown(bvstk_i2c_slave_hw_t *hardware)
{
    if (hardware == NULL) {
        return;
    }
    if (hardware->initialized != 0U) {
        bvstk_mmio_region_close(&hardware->bram);
        bvstk_mmio_region_close(&hardware->slave);
    }
    memset(hardware, 0, sizeof(*hardware));
}

bvstk_status_t bvstk_i2c_slave_hw_enable_irq(bvstk_i2c_slave_hw_t *hardware)
{
    if (hardware == NULL || hardware->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    return write32(&hardware->slave, BVSTK_I2C_SLV_IRQ_OFFSET, UINT32_C(1));
}

bvstk_status_t bvstk_i2c_slave_hw_capture_irq(
    bvstk_i2c_slave_hw_t *hardware,
    bvstk_i2c_slave_irq_event_t *event)
{
    uint32_t status_word = 0U;
    uint32_t tx_words;
    bvstk_status_t status;

    if (hardware == NULL || hardware->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (event == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    status = read32(&hardware->slave,
                    BVSTK_I2C_SLV_STATUS_OFFSET,
                    &status_word);
    if (status != BVSTK_OK) {
        return status;
    }
    tx_words = (status_word >> 5) & UINT32_C(0x7F);
    event->frame_size = tx_words > 0U ? (size_t)(tx_words - 1U) : 0U;
    if (event->frame_size > BVSTK_I2C_SLAVE_MAX_FRAME_BYTES) {
        event->frame_size = BVSTK_I2C_SLAVE_MAX_FRAME_BYTES;
    }
    event->read_phase = event->frame_size == 0U ? 1U : 0U;
    return write32(&hardware->slave, BVSTK_I2C_SLV_IRQ_OFFSET, UINT32_C(1));
}

bvstk_status_t bvstk_i2c_slave_hw_read_frame(
    const bvstk_i2c_slave_hw_t *hardware,
    size_t frame_size,
    uint8_t *frame,
    size_t frame_capacity)
{
    size_t i;

    if (hardware == NULL || hardware->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (frame_size > frame_capacity ||
        frame_size > BVSTK_I2C_SLAVE_MAX_FRAME_BYTES ||
        (frame_size != 0U && frame == NULL)) {
        return BVSTK_ERR_RANGE;
    }
    for (i = 0U; i < frame_size; ++i) {
        uint32_t word = 0U;
        bvstk_status_t status = read32(
            &hardware->bram,
            BVSTK_I2C_BRAM_SLAVE_WR_OFFSET + (i + 1U) * sizeof(uint32_t),
            &word);
        if (status != BVSTK_OK) {
            return status;
        }
        frame[i] = (uint8_t)(word & UINT32_C(0xFF));
    }
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_slave_hw_write_read_window(
    bvstk_i2c_slave_hw_t *hardware,
    const uint8_t *data,
    size_t size)
{
    size_t offset = 0U;

    if (hardware == NULL || hardware->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (size > BVSTK_I2C_BRAM_SIZE - BVSTK_I2C_BRAM_SLAVE_RD_OFFSET ||
        (size != 0U && data == NULL)) {
        return BVSTK_ERR_RANGE;
    }
    while (offset < size) {
        uint32_t word = 0U;
        size_t byte;
        size_t count = size - offset;
        if (count > sizeof(word)) {
            count = sizeof(word);
        }
        for (byte = 0U; byte < count; ++byte) {
            word |= (uint32_t)data[offset + byte] << (8U * byte);
        }
        if (write32(&hardware->bram,
                    BVSTK_I2C_BRAM_SLAVE_RD_OFFSET + offset,
                    word) != BVSTK_OK) {
            return BVSTK_ERR_IO;
        }
        offset += count;
    }
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_slave_hw_clear_frame(
    bvstk_i2c_slave_hw_t *hardware,
    size_t frame_size)
{
    size_t i;

    if (hardware == NULL || hardware->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (frame_size > BVSTK_I2C_SLAVE_MAX_FRAME_BYTES) {
        frame_size = BVSTK_I2C_SLAVE_MAX_FRAME_BYTES;
    }
    for (i = 0U; i < frame_size; ++i) {
        if (write32(&hardware->bram,
                    BVSTK_I2C_BRAM_SLAVE_WR_OFFSET +
                        (i + 1U) * sizeof(uint32_t),
                    0U) != BVSTK_OK) {
            return BVSTK_ERR_IO;
        }
    }
    return BVSTK_OK;
}
