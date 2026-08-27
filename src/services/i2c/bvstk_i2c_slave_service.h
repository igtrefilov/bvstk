#ifndef BVSTK_I2C_SLAVE_SERVICE_H
#define BVSTK_I2C_SLAVE_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "services/i2c/bvstk_i2c_cache.h"
#include "services/i2c/bvstk_i2c_devices.h"
#include "services/i2c/bvstk_i2c_master_service.h"
#include "shared/base/bvstk_status.h"

#define BVSTK_I2C_SLAVE_READ_WINDOW_BYTES 64U

/* Protocol state for one selected host-facing I2C slave device. */
typedef struct {
    bvstk_i2c_devices_t *devices;
    bvstk_i2c_cache_t *cache;
    bvstk_i2c_master_service_t *master;
    size_t target_device;
    uint8_t register_pointer[I2C_CFG_MAX_DEVICES];
    uint8_t read_armed[I2C_CFG_MAX_DEVICES];
    uint8_t initialized;
} bvstk_i2c_slave_service_t;

bvstk_status_t bvstk_i2c_slave_service_init(
    bvstk_i2c_slave_service_t *service,
    bvstk_i2c_devices_t *devices,
    bvstk_i2c_cache_t *cache,
    bvstk_i2c_master_service_t *master,
    size_t target_device);
void bvstk_i2c_slave_service_shutdown(bvstk_i2c_slave_service_t *service);
bvstk_status_t bvstk_i2c_slave_service_set_target(
    bvstk_i2c_slave_service_t *service,
    size_t target_device);
size_t bvstk_i2c_slave_service_target(
    const bvstk_i2c_slave_service_t *service);
void bvstk_i2c_slave_service_end_transaction(
    bvstk_i2c_slave_service_t *service);

/* Translate one hardware mailbox event into a response read window. */
bvstk_status_t bvstk_i2c_slave_service_handle_frame(
    bvstk_i2c_slave_service_t *service,
    const uint8_t *frame,
    size_t frame_size,
    uint8_t read_phase,
    uint8_t *read_window,
    size_t read_window_capacity,
    size_t *read_window_size,
    uint32_t timeout_ms);

#endif /* BVSTK_I2C_SLAVE_SERVICE_H */
