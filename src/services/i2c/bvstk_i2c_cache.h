#ifndef BVSTK_I2C_CACHE_H
#define BVSTK_I2C_CACHE_H

#include <stddef.h>
#include <stdint.h>

#include "shared/base/bvstk_status.h"
#include "shared/config/bvstk_config_model.h"
#include "shared/interfaces/bvstk_cache_sink.h"

typedef struct {
    bvstk_cache_sink_t sink;
    uint8_t values[I2C_CFG_MAX_DEVICES][I2C_CFG_MAX_REG_COUNT];
    uint8_t valid[I2C_CFG_MAX_DEVICES][I2C_CFG_MAX_REG_COUNT];
    uint8_t seeded[I2C_CFG_MAX_DEVICES][I2C_CFG_MAX_REG_COUNT];
    uint64_t updated_ms[I2C_CFG_MAX_DEVICES][I2C_CFG_MAX_REG_COUNT];
    size_t device_count;
    uint8_t initialized;
} bvstk_i2c_cache_t;

typedef struct {
    uint8_t value;
    uint8_t valid;
    uint8_t seeded;
    uint64_t updated_ms;
} bvstk_i2c_cache_entry_t;

bvstk_status_t bvstk_i2c_cache_init(bvstk_i2c_cache_t *cache,
                                    size_t device_count);
void bvstk_i2c_cache_shutdown(bvstk_i2c_cache_t *cache);
bvstk_status_t bvstk_i2c_cache_read(const bvstk_i2c_cache_t *cache,
                                    size_t device_id,
                                    uint8_t reg,
                                    uint8_t *value);
bvstk_status_t bvstk_i2c_cache_write(bvstk_i2c_cache_t *cache,
                                     size_t device_id,
                                     uint8_t reg,
                                     uint8_t value);
bvstk_status_t bvstk_i2c_cache_seed(bvstk_i2c_cache_t *cache,
                                    size_t device_id,
                                    uint8_t reg,
                                    uint8_t value);
bvstk_status_t bvstk_i2c_cache_invalidate(bvstk_i2c_cache_t *cache,
                                          size_t device_id,
                                          uint8_t reg);
bvstk_status_t bvstk_i2c_cache_entry(const bvstk_i2c_cache_t *cache,
                                     size_t device_id,
                                     uint8_t reg,
                                     bvstk_i2c_cache_entry_t *entry);
int bvstk_i2c_cache_is_valid(const bvstk_i2c_cache_t *cache,
                             size_t device_id,
                             uint8_t reg);

#endif /* BVSTK_I2C_CACHE_H */
