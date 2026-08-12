#include "services/i2c/bvstk_i2c_master_service.h"

#include <string.h>

#include "shared/interfaces/bvstk_clock.h"

static bvstk_status_t service_status(const bvstk_i2c_master_service_t *service,
                                     size_t device_id)
{
    if (service == NULL || service->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (device_id >= bvstk_i2c_devices_count(service->devices)) {
        return BVSTK_ERR_NOT_FOUND;
    }
    return BVSTK_OK;
}

static void emit(bvstk_i2c_master_service_t *service,
                 uint16_t type,
                 uint16_t status,
                 bvstk_event_source_t source,
                 bvstk_event_op_t operation,
                 const bvstk_i2c_device_t *device,
                 uint8_t reg,
                 uint8_t value)
{
    bvstk_event_t event;

    if (service == NULL || service->events.publish == NULL || device == NULL) {
        return;
    }
    memset(&event, 0, sizeof(event));
    event.time_us = bvstk_platform_now_ms() * UINT64_C(1000);
    event.type = type;
    event.status = status;
    event.source = (uint8_t)source;
    event.bus = BVSTK_EVENT_BUS_I2C;
    event.operation = (uint8_t)operation;
    event.arg0 = device->addr_7b;
    event.arg1 = reg;
    event.arg2 = value;
    service->events.publish(service->events.context, &event);
}

static bvstk_status_t backend_read(bvstk_i2c_master_service_t *service,
                                   const bvstk_i2c_device_t *device,
                                   uint8_t reg,
                                   uint8_t *value,
                                   uint32_t timeout_ms)
{
    if (service->io.read_reg != NULL) {
        return service->io.read_reg(service->io.context,
                                    device->addr_7b,
                                    reg,
                                    value,
                                    timeout_ms);
    }
    if (service->hardware == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_i2c_master_hw_read_reg(service->hardware,
                                        device->addr_7b,
                                        reg,
                                        value,
                                        timeout_ms);
}

static bvstk_status_t backend_write(bvstk_i2c_master_service_t *service,
                                    const bvstk_i2c_device_t *device,
                                    uint8_t reg,
                                    uint8_t value,
                                    uint32_t timeout_ms)
{
    if (service->io.write_reg != NULL) {
        return service->io.write_reg(service->io.context,
                                     device->addr_7b,
                                     reg,
                                     value,
                                     timeout_ms);
    }
    if (service->hardware == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_i2c_master_hw_write_reg(service->hardware,
                                         device->addr_7b,
                                         reg,
                                         value,
                                         timeout_ms);
}

bvstk_status_t bvstk_i2c_master_service_init(
    bvstk_i2c_master_service_t *service,
    bvstk_i2c_master_hw_t *hardware,
    const bvstk_i2c_master_io_t *io,
    bvstk_i2c_devices_t *devices,
    bvstk_i2c_cache_t *cache,
    bvstk_i2c_policy_t *policy,
    const bvstk_event_sink_t *events)
{
    if (service == NULL || devices == NULL || cache == NULL || policy == NULL ||
        devices->initialized == 0U || cache->initialized == 0U ||
        policy->initialized == 0U ||
        cache->device_count != devices->count ||
        policy->device_count != devices->count ||
        ((hardware == NULL || hardware->initialized == 0U) &&
         (io == NULL || io->read_reg == NULL || io->write_reg == NULL))) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(service, 0, sizeof(*service));
    service->hardware = hardware;
    if (io != NULL) {
        service->io = *io;
    }
    service->devices = devices;
    service->cache = cache;
    service->policy = policy;
    if (events != NULL) {
        service->events = *events;
    }
    service->initialized = 1U;
    return BVSTK_OK;
}

void bvstk_i2c_master_service_shutdown(bvstk_i2c_master_service_t *service)
{
    if (service != NULL) {
        memset(service, 0, sizeof(*service));
    }
}

size_t bvstk_i2c_master_service_device_count(
    const bvstk_i2c_master_service_t *service)
{
    return service != NULL && service->initialized != 0U
               ? bvstk_i2c_devices_count(service->devices)
               : 0U;
}

bvstk_status_t bvstk_i2c_master_service_device_info(
    const bvstk_i2c_master_service_t *service,
    size_t device_id,
    bvstk_i2c_device_t *out)
{
    bvstk_status_t status = service_status(service, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    return bvstk_i2c_devices_get(service->devices, device_id, out);
}

bvstk_status_t bvstk_i2c_master_service_find_by_name(
    const bvstk_i2c_master_service_t *service,
    const char *name,
    size_t *device_id)
{
    if (service == NULL || service->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_i2c_devices_find_by_name(service->devices, name, device_id);
}

bvstk_status_t bvstk_i2c_master_service_find_by_addr(
    const bvstk_i2c_master_service_t *service,
    uint8_t addr_7b,
    size_t *device_id)
{
    if (service == NULL || service->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_i2c_devices_find_by_addr(service->devices, addr_7b, device_id);
}

bvstk_status_t bvstk_i2c_master_service_read(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    uint8_t reg,
    uint8_t *value,
    uint32_t timeout_ms)
{
    bvstk_i2c_device_t device;
    bvstk_status_t status = service_status(service, device_id);

    if (status != BVSTK_OK) {
        return status;
    }
    if (value == NULL || bvstk_i2c_devices_get(service->devices,
                                               device_id,
                                               &device) != BVSTK_OK ||
        reg >= device.reg_count) {
        return BVSTK_ERR_RANGE;
    }
    status = backend_read(service, &device, reg, value, timeout_ms);
    if (status == BVSTK_OK) {
        (void)bvstk_i2c_cache_write(service->cache, device_id, reg, *value);
    }
    return status;
}

bvstk_status_t bvstk_i2c_master_service_read_cached(
    const bvstk_i2c_master_service_t *service,
    size_t device_id,
    uint8_t reg,
    uint8_t *value)
{
    bvstk_i2c_device_t device;
    bvstk_status_t status = service_status(service, device_id);

    if (status != BVSTK_OK) {
        return status;
    }
    if (value == NULL || bvstk_i2c_devices_get(service->devices,
                                               device_id,
                                               &device) != BVSTK_OK ||
        reg >= device.reg_count) {
        return BVSTK_ERR_RANGE;
    }
    return bvstk_i2c_cache_read(service->cache, device_id, reg, value);
}

bvstk_status_t bvstk_i2c_master_service_write(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    uint8_t reg,
    uint8_t value,
    bvstk_event_source_t source,
    uint32_t timeout_ms)
{
    bvstk_i2c_device_t device;
    bvstk_status_t status = service_status(service, device_id);

    if (status != BVSTK_OK) {
        return status;
    }
    if (bvstk_i2c_devices_get(service->devices, device_id, &device) != BVSTK_OK) {
        return BVSTK_ERR_NOT_FOUND;
    }
    if (reg >= device.reg_count || value > device.max_value_code) {
        return BVSTK_ERR_RANGE;
    }
    emit(service,
         BVSTK_EVENT_REG_ATTEMPT,
         0U,
         source,
         BVSTK_EVENT_OP_WRITE,
         &device,
         reg,
         value);
    if (!bvstk_i2c_policy_permits(service->policy,
                                  device_id,
                                  &device,
                                  reg,
                                  value)) {
        emit(service,
             BVSTK_EVENT_REG_DENIED,
             (uint16_t)BVSTK_ERR_DENIED,
             source,
             BVSTK_EVENT_OP_WRITE,
             &device,
             reg,
             value);
        return BVSTK_ERR_DENIED;
    }
    status = backend_write(service, &device, reg, value, timeout_ms);
    if (status != BVSTK_OK) {
        emit(service,
             BVSTK_EVENT_FAULT,
             (uint16_t)status,
             source,
             BVSTK_EVENT_OP_WRITE,
             &device,
             reg,
             value);
        return status;
    }
    (void)bvstk_i2c_cache_write(service->cache, device_id, reg, value);
    emit(service,
         BVSTK_EVENT_REG_COMMIT,
         0U,
         source,
         BVSTK_EVENT_OP_WRITE,
         &device,
         reg,
         value);
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_master_service_set_policy(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    i2c_policy_t mode)
{
    bvstk_status_t status = service_status(service, device_id);
    return status == BVSTK_OK
               ? bvstk_i2c_policy_set_mode(service->policy, device_id, mode)
               : status;
}

bvstk_status_t bvstk_i2c_master_service_get_policy(
    const bvstk_i2c_master_service_t *service,
    size_t device_id,
    bvstk_i2c_policy_entry_t *out)
{
    bvstk_status_t status = service_status(service, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    return bvstk_i2c_policy_get(service->policy, device_id, out);
}

bvstk_status_t bvstk_i2c_master_service_set_config(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    const i2c_device_config_t *config)
{
    bvstk_i2c_device_t device;
    bvstk_status_t status = service_status(service, device_id);

    if (status != BVSTK_OK) {
        return status;
    }
    if (config == NULL || config->name[0] == '\0' || config->reg_count == 0U ||
        config->reg_count > I2C_CFG_MAX_REG_COUNT ||
        config->addr_7b > UINT8_C(0x7F)) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(&device, 0, sizeof(device));
    memcpy(device.name, config->name, sizeof(device.name));
    device.name[sizeof(device.name) - 1U] = '\0';
    device.addr_7b = config->addr_7b;
    device.reg_count = config->reg_count;
    device.max_value_code = config->max_value_code;
    status = bvstk_i2c_devices_set(service->devices, device_id, &device);
    if (status != BVSTK_OK) {
        return status;
    }
    return bvstk_i2c_policy_set_config(service->policy, device_id, config);
}

bvstk_status_t bvstk_i2c_master_service_get_config(
    const bvstk_i2c_master_service_t *service,
    size_t device_id,
    i2c_device_config_t *out)
{
    bvstk_i2c_device_t device;
    bvstk_i2c_policy_entry_t policy;
    bvstk_status_t status = service_status(service, device_id);

    if (status != BVSTK_OK) {
        return status;
    }
    if (out == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    status = bvstk_i2c_devices_get(service->devices, device_id, &device);
    if (status != BVSTK_OK) {
        return status;
    }
    status = bvstk_i2c_policy_get(service->policy, device_id, &policy);
    if (status != BVSTK_OK) {
        return status;
    }
    memset(out, 0, sizeof(*out));
    memcpy(out->name, device.name, sizeof(out->name));
    out->name[sizeof(out->name) - 1U] = '\0';
    out->addr_7b = device.addr_7b;
    out->reg_count = device.reg_count;
    out->max_value_code = device.max_value_code;
    out->policy = policy.mode;
    memcpy(out->whitelist,
           policy.whitelist,
           policy.whitelist_len * sizeof(policy.whitelist[0]));
    out->whitelist_len = policy.whitelist_len;
    memcpy(out->blacklist,
           policy.blacklist,
           policy.blacklist_len * sizeof(policy.blacklist[0]));
    out->blacklist_len = policy.blacklist_len;
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_master_service_rule_allow(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    uint8_t reg,
    uint8_t value)
{
    bvstk_i2c_device_t device;
    bvstk_status_t status = service_status(service, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    status = bvstk_i2c_devices_get(service->devices, device_id, &device);
    return status == BVSTK_OK
               ? bvstk_i2c_policy_add_allow(service->policy,
                                            device_id,
                                            reg,
                                            value,
                                            &device)
               : status;
}

bvstk_status_t bvstk_i2c_master_service_rule_deny(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    uint8_t reg,
    uint8_t value)
{
    bvstk_i2c_device_t device;
    bvstk_status_t status = service_status(service, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    status = bvstk_i2c_devices_get(service->devices, device_id, &device);
    return status == BVSTK_OK
               ? bvstk_i2c_policy_add_deny(service->policy,
                                           device_id,
                                           reg,
                                           value,
                                           &device)
               : status;
}

bvstk_status_t bvstk_i2c_master_service_rule_clear(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    uint8_t reg,
    uint8_t value)
{
    bvstk_status_t status = service_status(service, device_id);
    return status == BVSTK_OK
               ? bvstk_i2c_policy_remove(service->policy, device_id, reg, value)
               : status;
}

bvstk_status_t bvstk_i2c_master_service_rule_clear_list(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    i2c_policy_t list)
{
    bvstk_status_t status = service_status(service, device_id);
    return status == BVSTK_OK
               ? bvstk_i2c_policy_clear(service->policy, device_id, list)
               : status;
}
