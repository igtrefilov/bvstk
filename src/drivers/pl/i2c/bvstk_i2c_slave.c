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

static size_t header_size(uint32_t header)
{
    return (size_t)((header & BVSTK_I2C_SLV_HEADER_BYTES_MASK) >>
                    BVSTK_I2C_SLV_HEADER_BYTES_SHIFT);
}

static uint8_t header_address(uint32_t header)
{
    return (uint8_t)(header & BVSTK_I2C_SLV_HEADER_ADDR_MASK);
}

static bvstk_status_t write_words(const bvstk_mmio_region_t *region,
                                  size_t base_offset,
                                  const uint8_t *data,
                                  size_t size)
{
    size_t offset = 0U;

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
        if (write32(region, base_offset + offset, word) != BVSTK_OK) {
            return BVSTK_ERR_IO;
        }
        offset += count;
    }
    return BVSTK_OK;
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

bvstk_status_t bvstk_i2c_slave_hw_set_address(
    bvstk_i2c_slave_hw_t *hardware,
    uint8_t addr_7b)
{
    uint32_t address_list[4] = {0U, 0U, 0U, 0U};
    size_t word;
    size_t bit;
    size_t i;

    if (hardware == NULL || hardware->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    addr_7b &= UINT8_C(0x7F);
    word = (size_t)addr_7b / 32U;
    bit = (size_t)addr_7b % 32U;
    address_list[word] = UINT32_C(1) << bit;
    for (i = 0U; i < 4U; ++i) {
        if (write32(&hardware->slave,
                    BVSTK_I2C_SLV_ADDR_LIST_0 + i * sizeof(uint32_t),
                    address_list[i]) != BVSTK_OK) {
            return BVSTK_ERR_IO;
        }
    }
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_slave_hw_clear_irq(bvstk_i2c_slave_hw_t *hardware)
{
    if (hardware == NULL || hardware->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    return write32(&hardware->slave,
                   BVSTK_I2C_SLV_IRQ_OFFSET,
                   BVSTK_I2C_SLV_IRQ_RESET_BIT);
}

bvstk_status_t bvstk_i2c_slave_hw_capture_irq(
    bvstk_i2c_slave_hw_t *hardware,
    bvstk_i2c_slave_irq_event_t *event)
{
    bvstk_status_t status;

    if (hardware == NULL || hardware->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (event == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(event, 0, sizeof(*event));
    status = read32(&hardware->slave,
                    BVSTK_I2C_SLV_IRQ_OFFSET,
                    &event->irq_flags);
    if (status != BVSTK_OK) {
        return status;
    }
    status = read32(&hardware->slave,
                    BVSTK_I2C_SLV_REQ_OFFSET,
                    &event->request_header);
    if (status != BVSTK_OK) {
        return status;
    }
    event->request_addr = header_address(event->request_header);
    event->request_size = header_size(event->request_header);
    return bvstk_i2c_slave_hw_clear_irq(hardware);
}

bvstk_status_t bvstk_i2c_slave_hw_read_frame(
    const bvstk_i2c_slave_hw_t *hardware,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *frame_size,
    uint8_t *addr_7b)
{
    uint32_t header = 0U;
    size_t size;
    size_t offset = 0U;

    if (hardware == NULL || hardware->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (frame_size == NULL || addr_7b == NULL ||
        (frame_capacity != 0U && frame == NULL)) {
        return BVSTK_ERR_MALFORMED;
    }
    *frame_size = 0U;
    *addr_7b = 0U;
    if (read32(&hardware->bram,
               BVSTK_I2C_BRAM_SLAVE_WR_OFFSET,
               &header) != BVSTK_OK) {
        return BVSTK_ERR_IO;
    }
    size = header_size(header);
    if (size > frame_capacity || size > BVSTK_I2C_SLAVE_MAX_FRAME_BYTES) {
        return BVSTK_ERR_RANGE;
    }
    *addr_7b = header_address(header);
    while (offset < size) {
        uint32_t word = 0U;
        size_t byte;
        size_t count = size - offset;

        if (count > sizeof(word)) {
            count = sizeof(word);
        }
        if (read32(&hardware->bram,
                   BVSTK_I2C_BRAM_SLAVE_WR_OFFSET + sizeof(uint32_t) + offset,
                   &word) != BVSTK_OK) {
            return BVSTK_ERR_IO;
        }
        for (byte = 0U; byte < count; ++byte) {
            frame[offset + byte] = (uint8_t)(word >> (8U * byte));
        }
        offset += count;
    }
    *frame_size = size;
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_slave_hw_write_read_window(
    bvstk_i2c_slave_hw_t *hardware,
    uint8_t addr_7b,
    const uint8_t *data,
    size_t size)
{
    uint32_t header;

    if (hardware == NULL || hardware->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (size > BVSTK_I2C_BRAM_SLAVE_WINDOW_SIZE - sizeof(uint32_t) ||
        (size != 0U && data == NULL)) {
        return BVSTK_ERR_RANGE;
    }
    addr_7b &= UINT8_C(0x7F);
    header = ((uint32_t)size << BVSTK_I2C_SLV_HEADER_BYTES_SHIFT) |
             (uint32_t)addr_7b;
    if (write32(&hardware->bram,
                BVSTK_I2C_BRAM_SLAVE_RD_OFFSET,
                header) != BVSTK_OK) {
        return BVSTK_ERR_IO;
    }
    return write_words(&hardware->bram,
                       BVSTK_I2C_BRAM_SLAVE_RD_OFFSET + sizeof(uint32_t),
                       data,
                       size);
}

bvstk_status_t bvstk_i2c_slave_hw_accept_read(
    bvstk_i2c_slave_hw_t *hardware)
{
    if (hardware == NULL || hardware->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    return write32(&hardware->slave,
                   BVSTK_I2C_SLV_CSR_OFFSET,
                   BVSTK_I2C_SLV_CSR_RD_VALID_BIT);
}

bvstk_status_t bvstk_i2c_slave_hw_clear_frame(
    bvstk_i2c_slave_hw_t *hardware,
    size_t frame_size)
{
    uint32_t zero = 0U;
    size_t offset = 0U;

    if (hardware == NULL || hardware->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (frame_size > BVSTK_I2C_SLAVE_MAX_FRAME_BYTES) {
        frame_size = BVSTK_I2C_SLAVE_MAX_FRAME_BYTES;
    }
    if (write32(&hardware->bram,
                BVSTK_I2C_BRAM_SLAVE_WR_OFFSET,
                zero) != BVSTK_OK) {
        return BVSTK_ERR_IO;
    }
    while (offset < frame_size) {
        if (write32(&hardware->bram,
                    BVSTK_I2C_BRAM_SLAVE_WR_OFFSET + sizeof(uint32_t) + offset,
                    zero) != BVSTK_OK) {
            return BVSTK_ERR_IO;
        }
        offset += sizeof(uint32_t);
    }
    return BVSTK_OK;
}
