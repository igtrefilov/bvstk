#include "services/i2c/bvstk_i2c_cache.h"

#include <string.h>

static bvstk_status_t cache_status(const bvstk_i2c_cache_t *cache,
                                   size_t device_id,
                                   uint8_t reg)
{
    (void)reg;
    if (cache == NULL || cache->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (device_id >= cache->device_count) {
        return BVSTK_ERR_RANGE;
    }
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_cache_init(bvstk_i2c_cache_t *cache,
                                    size_t device_count)
{
    if (cache == NULL || device_count > I2C_CFG_MAX_DEVICES) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(cache, 0, sizeof(*cache));
    cache->device_count = device_count;
    cache->initialized = 1U;
    return BVSTK_OK;
}

void bvstk_i2c_cache_shutdown(bvstk_i2c_cache_t *cache)
{
    if (cache != NULL) {
        memset(cache, 0, sizeof(*cache));
    }
}

bvstk_status_t bvstk_i2c_cache_read(const bvstk_i2c_cache_t *cache,
                                    size_t device_id,
                                    uint8_t reg,
                                    uint8_t *value)
{
    bvstk_status_t status = cache_status(cache, device_id, reg);
    if (status != BVSTK_OK) {
        return status;
    }
    if (value == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    *value = cache->values[device_id][reg];
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_cache_write(bvstk_i2c_cache_t *cache,
                                     size_t device_id,
                                     uint8_t reg,
                                     uint8_t value)
{
    bvstk_status_t status = cache_status(cache, device_id, reg);
    if (status != BVSTK_OK) {
        return status;
    }
    cache->values[device_id][reg] = value;
    cache->valid[device_id][reg] = 1U;
    return BVSTK_OK;
}

int bvstk_i2c_cache_is_valid(const bvstk_i2c_cache_t *cache,
                             size_t device_id,
                             uint8_t reg)
{
    return cache_status(cache, device_id, reg) == BVSTK_OK &&
           cache->valid[device_id][reg] != 0U;
}
