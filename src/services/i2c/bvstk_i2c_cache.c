#include "services/i2c/bvstk_i2c_cache.h"

#include <string.h>

#include "shared/interfaces/bvstk_clock.h"

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
    cache->seeded[device_id][reg] = 0U;
    cache->updated_ms[device_id][reg] = bvstk_platform_now_ms();
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_cache_seed(bvstk_i2c_cache_t *cache,
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
    cache->seeded[device_id][reg] = 1U;
    cache->updated_ms[device_id][reg] = bvstk_platform_now_ms();
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_cache_invalidate(bvstk_i2c_cache_t *cache,
                                          size_t device_id,
                                          uint8_t reg)
{
    bvstk_status_t status = cache_status(cache, device_id, reg);
    if (status != BVSTK_OK) {
        return status;
    }
    cache->valid[device_id][reg] = 0U;
    cache->seeded[device_id][reg] = 0U;
    cache->updated_ms[device_id][reg] = 0U;
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_cache_entry(const bvstk_i2c_cache_t *cache,
                                     size_t device_id,
                                     uint8_t reg,
                                     bvstk_i2c_cache_entry_t *entry)
{
    bvstk_status_t status = cache_status(cache, device_id, reg);
    if (status != BVSTK_OK) {
        return status;
    }
    if (entry == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    entry->value = cache->values[device_id][reg];
    entry->valid = cache->valid[device_id][reg];
    entry->seeded = cache->seeded[device_id][reg];
    entry->updated_ms = cache->updated_ms[device_id][reg];
    return BVSTK_OK;
}

int bvstk_i2c_cache_is_valid(const bvstk_i2c_cache_t *cache,
                             size_t device_id,
                             uint8_t reg)
{
    return cache_status(cache, device_id, reg) == BVSTK_OK &&
           cache->valid[device_id][reg] != 0U;
}
