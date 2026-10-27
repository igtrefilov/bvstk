#include "services/control/bvstk_control_api.h"
#include "shared/config/bvstk_i2c_config_codec.h"

#include <string.h>

bvstk_status_t bvstk_control_mem_check(const bvstk_control_api_t *api,
    uint32_t address, unsigned width, size_t count, int autoincrement, int writing)
{
    unsigned flag, bytes;
    uint64_t span;
    if (width != 8U && width != 16U && width != 32U && width != 64U)
        return BVSTK_ERR_UNSUPPORTED;
    flag = width == 8U ? BVSTK_MEMORY_8 : width == 16U ? BVSTK_MEMORY_16 :
           width == 32U ? BVSTK_MEMORY_32 : BVSTK_MEMORY_64;
    if (api == NULL || api->memory == NULL) return BVSTK_ERR_NOT_READY;
    if (!(api->memory->widths & flag) || api->memory->check == NULL ||
        (writing ? api->memory->write == NULL : api->memory->read == NULL))
        return BVSTK_ERR_UNSUPPORTED;
    bytes = width / 8U;
    if (!count || count > UINT32_MAX / bytes || (address & (bytes - 1U)))
        return BVSTK_ERR_RANGE;
    span = autoincrement ? (uint64_t)count * bytes : bytes;
    if ((uint64_t)address + span > (UINT64_C(1) << 32))
        return BVSTK_ERR_RANGE;
    return api->memory->check(api->memory->context, address, (size_t)span, width, writing);
}

bvstk_status_t bvstk_control_mem_read(const bvstk_control_api_t *api,
    uint32_t address, unsigned width, uint64_t *value)
{
    if (value == NULL) return BVSTK_ERR_MALFORMED;
    bvstk_status_t status = bvstk_control_mem_check(api, address, width, 1U, 0, 0);
    return status == BVSTK_OK ?
        api->memory->read(api->memory->context, address, width, value) : status;
}

bvstk_status_t bvstk_control_mem_write(const bvstk_control_api_t *api,
    uint32_t address, unsigned width, uint64_t value)
{
    bvstk_status_t status = bvstk_control_mem_check(api, address, width, 1U, 0, 1);
    return status == BVSTK_OK ?
        api->memory->write(api->memory->context, address, width, value) : status;
}

bvstk_status_t bvstk_control_i2c_snapshot(const bvstk_control_api_t *api,
    size_t device_id, const i2c_device_config_t *stored, int capture_cache,
    i2c_device_config_t *out)
{
    bvstk_i2c_device_t device;
    bvstk_i2c_policy_entry_t policy;
    bvstk_status_t status;
    if (stored == NULL || out == NULL) return BVSTK_ERR_MALFORMED;
    if (api == NULL || api->i2c == NULL) return BVSTK_ERR_NOT_READY;
    if (stored->settings_len > I2C_CFG_SETTINGS_MAX) return BVSTK_ERR_RANGE;
    status = bvstk_i2c_master_service_device_info(api->i2c, device_id, &device);
    if (status != BVSTK_OK) return status;
    status = bvstk_i2c_master_service_get_policy(api->i2c, device_id, &policy);
    if (status != BVSTK_OK) return status;
    *out = *stored;
    memcpy(out->name, device.name, sizeof(out->name));
    out->name[sizeof(out->name) - 1U] = '\0';
    out->addr_7b = device.addr_7b;
    out->reg_count = device.reg_count;
    out->max_value_code = device.max_value_code;
    out->presence_check_enabled = device.presence_check_enabled;
    out->presence_check_reg = device.presence_check_reg;
    out->endpoint = device.endpoint;
    out->policy = policy.mode;
    out->whitelist_len = policy.whitelist_len;
    memcpy(out->whitelist, policy.whitelist, policy.whitelist_len * sizeof(policy.whitelist[0]));
    out->blacklist_len = policy.blacklist_len;
    memcpy(out->blacklist, policy.blacklist, policy.blacklist_len * sizeof(policy.blacklist[0]));
    for (size_t reg = 0; capture_cache && reg < out->reg_count; ++reg) {
        bvstk_i2c_cache_entry_t entry;
        if (bvstk_i2c_master_service_cache_entry(api->i2c, device_id,
                (uint8_t)reg, &entry) != BVSTK_OK || !entry.valid) continue;
        size_t i;
        for (i = 0; i < out->settings_len; ++i)
            if (out->settings[i].reg == reg) break;
        if (i == out->settings_len) {
            if (i == I2C_CFG_SETTINGS_MAX) continue;
            ++out->settings_len;
        }
        out->settings[i].reg = (uint8_t)reg;
        out->settings[i].val = entry.value;
    }
    return BVSTK_OK;
}

bvstk_status_t bvstk_control_i2c_apply(bvstk_control_api_t *api,
    size_t device_id, const i2c_device_config_t *config)
{
    if (api == NULL || api->i2c == NULL) return BVSTK_ERR_NOT_READY;
    bvstk_status_t status = bvstk_i2c_config_validate(config);
    return status == BVSTK_OK ?
        bvstk_i2c_master_service_set_config(api->i2c, device_id, config) : status;
}

bvstk_status_t bvstk_control_pl_read32(bvstk_control_api_t *api,
                                       bvstk_pl_region_id_t region,
                                       size_t offset,
                                       uint32_t *value)
{
    if (api == NULL || api->pl == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_pl_service_read32(api->pl, region, offset, value);
}

bvstk_status_t bvstk_control_pl_write32(bvstk_control_api_t *api,
                                        bvstk_pl_region_id_t region,
                                        size_t offset,
                                        uint32_t value)
{
    if (api == NULL || api->pl == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_pl_service_write32(api->pl, region, offset, value);
}

bvstk_status_t bvstk_control_i2c_read(bvstk_control_api_t *api,
                                      size_t device_id,
                                      uint8_t reg,
                                      uint8_t *value,
                                      uint32_t timeout_ms)
{
    return bvstk_control_i2c_read_from(api, device_id, reg, value,
        BVSTK_EVENT_SOURCE_INTERNAL, timeout_ms);
}
bvstk_status_t bvstk_control_i2c_read_from(bvstk_control_api_t *api, size_t device_id,
    uint8_t reg, uint8_t *value, bvstk_event_source_t source, uint32_t timeout_ms)
{
    if (api == NULL || api->i2c == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_i2c_master_service_read_from(api->i2c,
                                         device_id,
                                         reg,
                                         value,
                                         source,
                                         timeout_ms);
}

bvstk_status_t bvstk_control_i2c_write(bvstk_control_api_t *api,
                                       size_t device_id,
                                       uint8_t reg,
                                       uint8_t value,
                                       bvstk_event_source_t source,
                                       uint32_t timeout_ms)
{
    if (api == NULL || api->i2c == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_i2c_master_service_write(api->i2c,
                                          device_id,
                                          reg,
                                          value,
                                          source,
                                          timeout_ms);
}

bvstk_status_t bvstk_control_mdio_read(bvstk_control_api_t *api,
                                      size_t device_id,
                                      uint8_t reg,
                                      uint16_t *value,
                                      uint32_t timeout_ms)
{
    return bvstk_control_mdio_read_from(api, device_id, reg, value,
        BVSTK_EVENT_SOURCE_INTERNAL, timeout_ms);
}
bvstk_status_t bvstk_control_mdio_read_from(bvstk_control_api_t *api, size_t device_id,
    uint8_t reg, uint16_t *value, bvstk_event_source_t source, uint32_t timeout_ms)
{
    if (api == NULL || api->mdio == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_mdio_service_read_from(api->mdio,
                                  device_id,
                                  reg,
                                  value,
                                  source,
                                  timeout_ms);
}

bvstk_status_t bvstk_control_mdio_write(bvstk_control_api_t *api,
                                       size_t device_id,
                                       uint8_t reg,
                                       uint16_t value,
                                       bvstk_event_source_t source,
                                       uint32_t timeout_ms)
{
    if (api == NULL || api->mdio == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_mdio_service_write(api->mdio,
                                   device_id,
                                   reg,
                                   value,
                                   source,
                                   timeout_ms);
}

bvstk_status_t bvstk_control_spi_transfer(bvstk_control_api_t *api,
                                          const uint32_t *tx_words,
                                          size_t tx_count,
                                          uint32_t *rx_words,
                                          size_t rx_capacity,
                                          uint32_t timeout_ms)
{
    if (api == NULL || api->spi == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_spi_core_transfer(api->spi,
                                   tx_words,
                                   tx_count,
                                   rx_words,
                                   rx_capacity,
                                   timeout_ms);
}
