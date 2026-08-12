#include "services/smi/bvstk_smi_service.h"

#include <string.h>
#include <strings.h>

static bvstk_status_t device_status(const bvstk_smi_service_t *service,
                                    size_t device_id)
{
    if (service == NULL || service->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    return device_id < service->device_count ? BVSTK_OK : BVSTK_ERR_NOT_FOUND;
}

static void emit(bvstk_smi_service_t *service,
                 uint16_t type,
                 uint16_t status,
                 bvstk_event_source_t source,
                 bvstk_event_op_t operation,
                 const smi_phy_config_t *config,
                 uint8_t reg,
                 uint16_t value)
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
    event.bus = BVSTK_EVENT_BUS_SMI;
    event.operation = (uint8_t)operation;
    event.arg0 = config->phy_addr & UINT8_C(0x1F);
    event.arg1 = reg;
    event.arg2 = value;
    service->events.publish(service->events.context, &event);
}

static int contains(const uint8_t *values, size_t length, uint8_t value)
{
    size_t i;
    if (values == NULL) {
        return 0;
    }
    for (i = 0U; i < length; ++i) {
        if (values[i] == value) {
            return 1;
        }
    }
    return 0;
}

static int permitted(const smi_phy_config_t *config, uint8_t reg)
{
    if (config == NULL || reg >= config->reg_count) {
        return 0;
    }
    if (config->policy == SMI_POLICY_WHITELIST) {
        return contains(config->write_allow_regs,
                        config->write_allow_regs_len,
                        reg);
    }
    return !contains(config->write_deny_regs,
                     config->write_deny_regs_len,
                     reg);
}

static bvstk_status_t backend_read(bvstk_smi_service_t *service,
                                   const smi_phy_config_t *config,
                                   uint8_t reg,
                                   uint16_t *value,
                                   uint32_t timeout_ms)
{
    if (service->bus.read != NULL) {
        return service->bus.read(service->bus.context,
                                 config->phy_addr,
                                 reg,
                                 value,
                                 timeout_ms);
    }
    if (service->core == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_smi_core_read(service->core,
                               config->phy_addr,
                               reg,
                               value,
                               timeout_ms);
}

static bvstk_status_t backend_write(bvstk_smi_service_t *service,
                                    const smi_phy_config_t *config,
                                    uint8_t reg,
                                    uint16_t value,
                                    bvstk_event_source_t source,
                                    uint32_t timeout_ms)
{
    if (service->bus.write != NULL) {
        return service->bus.write(service->bus.context,
                                  config->phy_addr,
                                  reg,
                                  value,
                                  source,
                                  timeout_ms);
    }
    if (service->core == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_smi_core_write(service->core,
                                config->phy_addr,
                                reg,
                                value,
                                timeout_ms);
}

static void update_setting(smi_phy_config_t *config,
                           uint8_t reg,
                           uint16_t value)
{
    size_t i;
    for (i = 0U; i < config->settings_len; ++i) {
        if (config->settings[i].reg == reg) {
            config->settings[i].val = value;
            return;
        }
    }
    if (config->settings_len < SMI_CFG_SETTINGS_MAX) {
        config->settings[config->settings_len].reg = reg;
        config->settings[config->settings_len].val = value;
        config->settings_len++;
    }
}

bvstk_status_t bvstk_smi_service_init(bvstk_smi_service_t *service,
                                      bvstk_smi_core_t *core,
                                      const bvstk_smi_bus_ops_t *bus,
                                      const smi_phy_config_t *devices,
                                      size_t device_count,
                                      const bvstk_event_sink_t *events)
{
    if (service == NULL || device_count > SMI_CFG_MAX_DEVICES ||
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

void bvstk_smi_service_shutdown(bvstk_smi_service_t *service)
{
    if (service != NULL) {
        memset(service, 0, sizeof(*service));
    }
}

size_t bvstk_smi_service_device_count(const bvstk_smi_service_t *service)
{
    return service != NULL && service->initialized != 0U
               ? service->device_count
               : 0U;
}

bvstk_status_t bvstk_smi_service_device_info(const bvstk_smi_service_t *service,
                                             size_t device_id,
                                             bvstk_smi_device_info_t *out)
{
    bvstk_status_t status = device_status(service, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (out == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    out->name = service->devices[device_id].name;
    out->phy_addr = service->devices[device_id].phy_addr;
    out->reg_count = service->devices[device_id].reg_count;
    out->policy = service->devices[device_id].policy;
    return BVSTK_OK;
}

bvstk_status_t bvstk_smi_service_find_by_name(const bvstk_smi_service_t *service,
                                              const char *name,
                                              size_t *device_id)
{
    size_t i;
    if (service == NULL || name == NULL || device_id == NULL) {
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

bvstk_status_t bvstk_smi_service_find_by_phy(const bvstk_smi_service_t *service,
                                             uint8_t phy_addr,
                                             size_t *device_id)
{
    size_t i;
    if (service == NULL || device_id == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    phy_addr &= UINT8_C(0x1F);
    for (i = 0U; i < service->device_count; ++i) {
        if ((service->devices[i].phy_addr & UINT8_C(0x1F)) == phy_addr) {
            *device_id = i;
            return BVSTK_OK;
        }
    }
    return BVSTK_ERR_NOT_FOUND;
}

bvstk_status_t bvstk_smi_service_read(bvstk_smi_service_t *service,
                                      size_t device_id,
                                      uint8_t reg,
                                      uint16_t *value,
                                      uint32_t timeout_ms)
{
    bvstk_status_t status = device_status(service, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (value == NULL || reg >= service->devices[device_id].reg_count ||
        reg >= 32U) {
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

bvstk_status_t bvstk_smi_service_read_cached(const bvstk_smi_service_t *service,
                                             size_t device_id,
                                             uint8_t reg,
                                             uint16_t *value)
{
    bvstk_status_t status = device_status(service, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (value == NULL || reg >= service->devices[device_id].reg_count ||
        reg >= 32U) {
        return BVSTK_ERR_RANGE;
    }
    *value = service->cache[device_id][reg];
    return BVSTK_OK;
}

bvstk_status_t bvstk_smi_service_write(bvstk_smi_service_t *service,
                                       size_t device_id,
                                       uint8_t reg,
                                       uint16_t value,
                                       bvstk_event_source_t source,
                                       uint32_t timeout_ms)
{
    smi_phy_config_t *config;
    bvstk_status_t status = device_status(service, device_id);

    if (status != BVSTK_OK) {
        return status;
    }
    config = &service->devices[device_id];
    emit(service, BVSTK_EVENT_REG_ATTEMPT, 0U, source,
         BVSTK_EVENT_OP_WRITE, config, reg, value);
    if (!permitted(config, reg)) {
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

bvstk_status_t bvstk_smi_service_set_policy(bvstk_smi_service_t *service,
                                             size_t device_id,
                                             smi_policy_t policy)
{
    bvstk_status_t status = device_status(service, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (policy != SMI_POLICY_WHITELIST && policy != SMI_POLICY_BLACKLIST) {
        return BVSTK_ERR_MALFORMED;
    }
    service->devices[device_id].policy = policy;
    return BVSTK_OK;
}

bvstk_status_t bvstk_smi_service_get_config(const bvstk_smi_service_t *service,
                                             size_t device_id,
                                             smi_phy_config_t *out)
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

bvstk_status_t bvstk_smi_service_set_config(bvstk_smi_service_t *service,
                                             size_t device_id,
                                             const smi_phy_config_t *config)
{
    bvstk_status_t status = device_status(service, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (config == NULL || config->name[0] == '\0' || config->reg_count == 0U ||
        config->reg_count > 32U || config->phy_addr > UINT8_C(0x1F)) {
        return BVSTK_ERR_MALFORMED;
    }
    service->devices[device_id] = *config;
    return BVSTK_OK;
}

bvstk_status_t bvstk_smi_service_poll(bvstk_smi_service_t *service,
                                      uint32_t timeout_ms)
{
    size_t device_id;
    if (service == NULL || service->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    for (device_id = 0U; device_id < service->device_count; ++device_id) {
        smi_phy_config_t *config = &service->devices[device_id];
        size_t i;
        if (!config->autopoll_enabled) {
            continue;
        }
        if (config->autopoll_regs_len == 0U) {
            continue;
        }
        for (i = 0U; i < config->autopoll_regs_len; ++i) {
            uint16_t value = 0U;
            bvstk_status_t status = bvstk_smi_service_read(service,
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
