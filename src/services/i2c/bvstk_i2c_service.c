#include "services/i2c/bvstk_i2c_service.h"

#include <string.h>
#include <strings.h>

static bvstk_status_t device_status(const bvstk_i2c_service_t *service,
                                    size_t device_id)
{
    if (service == NULL || service->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (device_id >= service->device_count) {
        return BVSTK_ERR_NOT_FOUND;
    }
    return BVSTK_OK;
}

static void emit(bvstk_i2c_service_t *service,
                 uint16_t type,
                 uint16_t status,
                 bvstk_event_source_t source,
                 bvstk_event_op_t operation,
                 const i2c_device_config_t *config,
                 uint8_t reg,
                 uint8_t value)
{
    bvstk_event_t event;

    if (service == NULL || service->events.publish == NULL || config == NULL) {
        return;
    }
    memset(&event, 0, sizeof(event));
    event.time_us = bvstk_platform_now_ms() * UINT64_C(1000);
    event.type = type;
    event.status = status;
    event.source = (uint8_t)source;
    event.bus = BVSTK_EVENT_BUS_I2C;
    event.operation = (uint8_t)operation;
    event.arg0 = config->addr_7b;
    event.arg1 = reg;
    event.arg2 = value;
    service->events.publish(service->events.context, &event);
}

static bool rule_contains(const i2c_rule_entry_t *rules,
                          size_t length,
                          uint8_t reg,
                          uint8_t value)
{
    size_t i;
    if (rules == NULL) {
        return false;
    }
    for (i = 0U; i < length; ++i) {
        if (rules[i].reg == reg && rules[i].val == value) {
            return true;
        }
    }
    return false;
}

static bool permitted(const i2c_device_config_t *config,
                      uint8_t reg,
                      uint8_t value)
{
    if (config == NULL || reg >= config->reg_count ||
        value > config->max_value_code) {
        return false;
    }
    if (config->policy == I2C_POLICY_WHITELIST) {
        return rule_contains(config->whitelist,
                             config->whitelist_len,
                             reg,
                             value);
    }
    return !rule_contains(config->blacklist,
                          config->blacklist_len,
                          reg,
                          value);
}

static bvstk_status_t backend_read(bvstk_i2c_service_t *service,
                                   const i2c_device_config_t *config,
                                   uint8_t reg,
                                   uint8_t *value,
                                   uint32_t timeout_ms)
{
    if (service->bus.read_reg != NULL) {
        return service->bus.read_reg(service->bus.context,
                                     config->addr_7b,
                                     reg,
                                     value,
                                     timeout_ms);
    }
    if (service->core == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_i2c_core_read_reg(service->core,
                                   config->addr_7b,
                                   reg,
                                   value,
                                   timeout_ms);
}

static bvstk_status_t backend_write(bvstk_i2c_service_t *service,
                                    const i2c_device_config_t *config,
                                    uint8_t reg,
                                    uint8_t value,
                                    bvstk_event_source_t source,
                                    uint32_t timeout_ms)
{
    if (service->bus.write_reg != NULL) {
        return service->bus.write_reg(service->bus.context,
                                      config->addr_7b,
                                      reg,
                                      value,
                                      source,
                                      timeout_ms);
    }
    if (service->core == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_i2c_core_write_reg(service->core,
                                    config->addr_7b,
                                    reg,
                                    value,
                                    timeout_ms);
}

static void update_setting(i2c_device_config_t *config,
                           uint8_t reg,
                           uint8_t value)
{
    size_t i;

    for (i = 0U; i < config->settings_len; ++i) {
        if (config->settings[i].reg == reg) {
            config->settings[i].val = value;
            return;
        }
    }
    if (config->settings_len < I2C_CFG_SETTINGS_MAX) {
        config->settings[config->settings_len].reg = reg;
        config->settings[config->settings_len].val = value;
        config->settings_len++;
    }
}

bvstk_status_t bvstk_i2c_service_init(bvstk_i2c_service_t *service,
                                      bvstk_i2c_core_t *core,
                                      const bvstk_i2c_bus_ops_t *bus,
                                      const i2c_device_config_t *devices,
                                      size_t device_count,
                                      const bvstk_event_sink_t *events)
{
    if (service == NULL || device_count > I2C_CFG_MAX_DEVICES ||
        (device_count != 0U && devices == NULL)) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(service, 0, sizeof(*service));
    service->core = core;
    if (bus != NULL) {
        service->bus = *bus;
    }
    if (events != NULL) {
        service->events = *events;
    }
    if (device_count != 0U) {
        memcpy(service->devices,
               devices,
               device_count * sizeof(service->devices[0]));
    }
    service->device_count = device_count;
    service->initialized = 1U;
    return BVSTK_OK;
}

void bvstk_i2c_service_shutdown(bvstk_i2c_service_t *service)
{
    if (service != NULL) {
        memset(service, 0, sizeof(*service));
    }
}

size_t bvstk_i2c_service_device_count(const bvstk_i2c_service_t *service)
{
    return service != NULL && service->initialized != 0U
               ? service->device_count
               : 0U;
}

bvstk_status_t bvstk_i2c_service_device_info(const bvstk_i2c_service_t *service,
                                             size_t device_id,
                                             bvstk_i2c_device_info_t *out)
{
    bvstk_status_t status = device_status(service, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (out == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    out->name = service->devices[device_id].name;
    out->addr_7b = service->devices[device_id].addr_7b;
    out->reg_count = service->devices[device_id].reg_count;
    out->max_value_code = service->devices[device_id].max_value_code;
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_service_find_by_name(const bvstk_i2c_service_t *service,
                                              const char *name,
                                              size_t *device_id)
{
    size_t i;
    if (service == NULL || name == NULL || name[0] == '\0' || device_id == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    for (i = 0U; i < service->device_count; ++i) {
        if (strcasecmp(service->devices[i].name, name) == 0) {
            *device_id = i;
            return BVSTK_OK;
        }
    }
    return BVSTK_ERR_NOT_FOUND;
}

bvstk_status_t bvstk_i2c_service_find_by_addr(const bvstk_i2c_service_t *service,
                                              uint8_t addr_7b,
                                              size_t *device_id)
{
    size_t i;
    if (service == NULL || device_id == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    addr_7b &= UINT8_C(0x7F);
    for (i = 0U; i < service->device_count; ++i) {
        if (service->devices[i].addr_7b == addr_7b) {
            *device_id = i;
            return BVSTK_OK;
        }
    }
    return BVSTK_ERR_NOT_FOUND;
}

bvstk_status_t bvstk_i2c_service_read_reg(bvstk_i2c_service_t *service,
                                          size_t device_id,
                                          uint8_t reg,
                                          uint8_t *value,
                                          uint32_t timeout_ms)
{
    bvstk_status_t status = device_status(service, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (value == NULL || reg >= service->devices[device_id].reg_count) {
        return BVSTK_ERR_RANGE;
    }
    status = backend_read(service,
                          &service->devices[device_id],
                          reg,
                          value,
                          timeout_ms);
    if (status == BVSTK_OK) {
        service->cache[device_id][reg] = *value;
    }
    return status;
}

bvstk_status_t bvstk_i2c_service_read_cached(const bvstk_i2c_service_t *service,
                                             size_t device_id,
                                             uint8_t reg,
                                             uint8_t *value)
{
    bvstk_status_t status = device_status(service, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (value == NULL || reg >= service->devices[device_id].reg_count) {
        return BVSTK_ERR_RANGE;
    }
    *value = service->cache[device_id][reg];
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_service_write_reg(bvstk_i2c_service_t *service,
                                           size_t device_id,
                                           uint8_t reg,
                                           uint8_t value,
                                           bvstk_event_source_t source,
                                           uint32_t timeout_ms)
{
    i2c_device_config_t *config;
    bvstk_status_t status = device_status(service, device_id);

    if (status != BVSTK_OK) {
        return status;
    }
    config = &service->devices[device_id];
    emit(service, BVSTK_EVENT_REG_ATTEMPT, 0U, source,
         BVSTK_EVENT_OP_WRITE, config, reg, value);
    if (!permitted(config, reg, value)) {
        emit(service, BVSTK_EVENT_REG_DENIED, (uint16_t)BVSTK_ERR_DENIED,
             source, BVSTK_EVENT_OP_WRITE, config, reg, value);
        return BVSTK_ERR_DENIED;
    }

    status = backend_write(service, config, reg, value, source, timeout_ms);
    if (status != BVSTK_OK) {
        emit(service, BVSTK_EVENT_FAULT, (uint16_t)status, source,
             BVSTK_EVENT_OP_WRITE, config, reg, value);
        return status;
    }
    service->cache[device_id][reg] = value;
    update_setting(config, reg, value);
    emit(service, BVSTK_EVENT_REG_COMMIT, 0U, source,
         BVSTK_EVENT_OP_WRITE, config, reg, value);
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_service_set_policy(bvstk_i2c_service_t *service,
                                            size_t device_id,
                                            i2c_policy_t policy)
{
    bvstk_status_t status = device_status(service, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (policy != I2C_POLICY_WHITELIST && policy != I2C_POLICY_BLACKLIST) {
        return BVSTK_ERR_MALFORMED;
    }
    service->devices[device_id].policy = policy;
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_service_get_config(const bvstk_i2c_service_t *service,
                                            size_t device_id,
                                            i2c_device_config_t *out)
{
    bvstk_status_t status = device_status(service, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (out == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    *out = service->devices[device_id];
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_service_set_config(bvstk_i2c_service_t *service,
                                            size_t device_id,
                                            const i2c_device_config_t *config)
{
    bvstk_status_t status = device_status(service, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (config == NULL || config->name[0] == '\0' || config->reg_count == 0U ||
        config->reg_count > I2C_CFG_MAX_REG_COUNT ||
        config->addr_7b > UINT8_C(0x7F)) {
        return BVSTK_ERR_MALFORMED;
    }
    service->devices[device_id] = *config;
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_service_poll(bvstk_i2c_service_t *service,
                                      uint32_t timeout_ms)
{
    size_t device_id;

    if (service == NULL || service->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    for (device_id = 0U; device_id < service->device_count; ++device_id) {
        i2c_device_config_t *config = &service->devices[device_id];
        size_t i;
        if (!config->autopoll_enabled) {
            continue;
        }
        for (i = 0U; i < config->autopoll_regs_len; ++i) {
            uint8_t value = 0U;
            bvstk_status_t status = bvstk_i2c_service_read_reg(service,
                                                                device_id,
                                                                config->autopoll_regs[i],
                                                                &value,
                                                                timeout_ms);
            if (status != BVSTK_OK) {
                return status;
            }
        }
    }
    return BVSTK_OK;
}
