#ifndef BVSTK_I2C_SLAVE_H
#define BVSTK_I2C_SLAVE_H

#include <stddef.h>
#include <stdint.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "shared/base/bvstk_status.h"
#include "shared/interfaces/bvstk_mmio.h"

/* The develop slave stores a header followed by up to one BRAM window of data. */
#define BVSTK_I2C_SLAVE_MAX_FRAME_BYTES 256U

typedef struct {
    bvstk_mmio_region_t slave;
    bvstk_mmio_region_t bram;
    uint8_t initialized;
} bvstk_i2c_slave_hw_t;

typedef struct {
    uint32_t irq_flags;
    uint32_t request_header;
    uint8_t request_addr;
    size_t request_size;
} bvstk_i2c_slave_irq_event_t;

bvstk_status_t bvstk_i2c_slave_hw_init(bvstk_i2c_slave_hw_t *hardware);
void bvstk_i2c_slave_hw_shutdown(bvstk_i2c_slave_hw_t *hardware);
bvstk_status_t bvstk_i2c_slave_hw_set_address(
    bvstk_i2c_slave_hw_t *hardware,
    uint8_t addr_7b);
bvstk_status_t bvstk_i2c_slave_hw_clear_irq(bvstk_i2c_slave_hw_t *hardware);
bvstk_status_t bvstk_i2c_slave_hw_capture_irq(
    bvstk_i2c_slave_hw_t *hardware,
    bvstk_i2c_slave_irq_event_t *event);
bvstk_status_t bvstk_i2c_slave_hw_read_frame(
    const bvstk_i2c_slave_hw_t *hardware,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *frame_size,
    uint8_t *addr_7b);
bvstk_status_t bvstk_i2c_slave_hw_write_read_window(
    bvstk_i2c_slave_hw_t *hardware,
    uint8_t addr_7b,
    const uint8_t *data,
    size_t size);
bvstk_status_t bvstk_i2c_slave_hw_accept_read(
    bvstk_i2c_slave_hw_t *hardware);
bvstk_status_t bvstk_i2c_slave_hw_clear_frame(
    bvstk_i2c_slave_hw_t *hardware,
    size_t frame_size);

#endif /* BVSTK_I2C_SLAVE_H */
