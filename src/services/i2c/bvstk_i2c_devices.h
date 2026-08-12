#ifndef BVSTK_I2C_DEVICES_H
#define BVSTK_I2C_DEVICES_H

#include <stddef.h>
#include <stdint.h>

#include "shared/base/bvstk_status.h"
#include "shared/config/bvstk_config_model.h"

/* Runtime descriptors contain only bus-facing device identity and limits. */
typedef struct {
    char name[I2C_CFG_NAME_MAX];
    uint8_t addr_7b;
    uint16_t reg_count;
    uint8_t max_value_code;
} bvstk_i2c_device_t;

typedef struct {
    bvstk_i2c_device_t items[I2C_CFG_MAX_DEVICES];
    size_t count;
    uint8_t initialized;
} bvstk_i2c_devices_t;

bvstk_status_t bvstk_i2c_devices_init(bvstk_i2c_devices_t *devices,
                                      const bvstk_i2c_device_t *items,
                                      size_t count);
bvstk_status_t bvstk_i2c_devices_init_from_config(
    bvstk_i2c_devices_t *devices,
    const i2c_device_config_t *configs,
    size_t count);
void bvstk_i2c_devices_shutdown(bvstk_i2c_devices_t *devices);

size_t bvstk_i2c_devices_count(const bvstk_i2c_devices_t *devices);
bvstk_status_t bvstk_i2c_devices_get(const bvstk_i2c_devices_t *devices,
                                     size_t device_id,
                                     bvstk_i2c_device_t *out);
bvstk_status_t bvstk_i2c_devices_set(bvstk_i2c_devices_t *devices,
                                     size_t device_id,
                                     const bvstk_i2c_device_t *item);
bvstk_status_t bvstk_i2c_devices_find_by_name(
    const bvstk_i2c_devices_t *devices,
    const char *name,
    size_t *device_id);
bvstk_status_t bvstk_i2c_devices_find_by_addr(
    const bvstk_i2c_devices_t *devices,
    uint8_t addr_7b,
    size_t *device_id);

#endif /* BVSTK_I2C_DEVICES_H */
