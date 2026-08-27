#ifndef BVSTK_I2C_CONFIG_CODEC_H
#define BVSTK_I2C_CONFIG_CODEC_H

#include <stddef.h>

#include "shared/base/bvstk_status.h"
#include "shared/config/bvstk_config_model.h"

/* Portable JSON codec shared by RTOS-specific configuration stores. */
bvstk_status_t bvstk_i2c_config_parse_json(const char *json,
                                           i2c_device_config_t *config);
bvstk_status_t bvstk_i2c_config_validate(
    const i2c_device_config_t *config);
bvstk_status_t bvstk_i2c_config_serialize_json(
    const i2c_device_config_t *config,
    char *json,
    size_t json_capacity,
    size_t *json_size);

#endif /* BVSTK_I2C_CONFIG_CODEC_H */
