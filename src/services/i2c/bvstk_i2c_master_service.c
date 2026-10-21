#include "services/i2c/bvstk_i2c_master_service.h"

#include <string.h>

#include "shared/interfaces/bvstk_clock.h"
#include "shared/config/bvstk_i2c_config_codec.h"

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

static uint8_t wire_address(const bvstk_i2c_device_t *device, unsigned reg)
{
    const bvstk_address_space_t *space = bvstk_i2c_register_space(&device->endpoint, reg);
    return space ? space->address : device->addr_7b;
}

static void record_response(bvstk_i2c_master_service_t *service,
                             size_t device_id, uint8_t address,
                             bvstk_status_t status, int read)
{
    bvstk_i2c_device_t current;
    int space;
    bvstk_cache_sink_enter(&service->cache->sink);
    if (bvstk_i2c_devices_get(service->devices, device_id, &current) != BVSTK_OK ||
        (space = bvstk_i2c_address_space(&current.endpoint, current.addr_7b, address)) < 0) {
        bvstk_cache_sink_leave(&service->cache->sink);
        return;
    }
    uint8_t *presence = current.endpoint.space_count ?
        &service->space_presence[device_id][space] : &service->presence[device_id];
    if (status == BVSTK_OK && read)
        *presence = BVSTK_I2C_PRESENCE_RESPONDING;
    else if (status == BVSTK_ERR_IO || status == BVSTK_ERR_TIMEOUT ||
             status == BVSTK_ERR_NOT_FOUND)
        *presence = BVSTK_I2C_PRESENCE_UNAVAILABLE;
    /* The first window establishes device readiness. Optional secondary
     * windows have independent presence and cannot hide a responding device. */
    if (space == 0) service->presence[device_id] = *presence;
    bvstk_cache_sink_presence(&service->cache->sink, device_id, service->presence[device_id]);
    bvstk_cache_sink_leave(&service->cache->sink);
}

static void cache_response(bvstk_i2c_master_service_t *service, size_t device_id,
    const bvstk_i2c_device_t *target, uint8_t reg, uint8_t value, bvstk_status_t status)
{
    bvstk_i2c_device_t current;
    bvstk_cache_sink_enter(&service->cache->sink);
    /* A configuration change can complete while hardware I/O is waiting.
     * Keep a reply for the old address out of the new device's cache. */
    if (bvstk_i2c_devices_get(service->devices, device_id, &current) == BVSTK_OK &&
        current.addr_7b == target->addr_7b && !strcmp(current.name, target->name) &&
        !memcmp(&current.endpoint, &target->endpoint, sizeof(current.endpoint)) && reg < current.reg_count) {
        if (status == BVSTK_OK) (void)bvstk_i2c_cache_write(service->cache, device_id, reg, value);
        else (void)bvstk_i2c_cache_invalidate(service->cache, device_id, reg);
    }
    bvstk_cache_sink_leave(&service->cache->sink);
}

static void emit_flags(bvstk_i2c_master_service_t *service,
                 uint16_t type,
                 uint16_t status,
                 bvstk_event_source_t source,
                 bvstk_event_op_t operation,
                 const bvstk_i2c_device_t *device,
                 uint32_t reg,
                 uint8_t value,
                 uint8_t flags)
{
    bvstk_event_t event;
    size_t id;

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
    event.flags = flags;
    if (bvstk_i2c_devices_find_by_name(service->devices, device->name, &id) == BVSTK_OK)
        event.flags |= BVSTK_EVENT_FLAG_DEVICE_SLOT | (uint8_t)(id << BVSTK_EVENT_DEVICE_SHIFT);
    const bvstk_address_space_t *space = bvstk_i2c_register_space(&device->endpoint, reg);
    event.arg0 = space ? space->address : device->addr_7b;
    event.arg1 = reg - (space ? space->first : 0U);
    event.arg2 = value;
    service->events.publish(service->events.context, &event);
}

static void emit(bvstk_i2c_master_service_t *service,
                 uint16_t type,
                 uint16_t status,
                 bvstk_event_source_t source,
                 bvstk_event_op_t operation,
                 const bvstk_i2c_device_t *device,
                 uint32_t reg,
                 uint8_t value)
{
    emit_flags(service, type, status, source, operation, device, reg, value,
               BVSTK_EVENT_FLAG_NONE);
}

static int presence_confirmed(const bvstk_i2c_master_service_t *service,
                              size_t device_id, unsigned reg)
{
    if (!service || device_id >= service->devices->count) return 0;
    const bvstk_i2c_device_t *d = &service->devices->items[device_id];
    int space = bvstk_i2c_address_space(&d->endpoint, d->addr_7b, wire_address(d, reg));
    return space >= 0 && (d->endpoint.space_count ? service->space_presence[device_id][space] :
        service->presence[device_id]) == BVSTK_I2C_PRESENCE_RESPONDING;
}

static bvstk_status_t raw_transfer(bvstk_i2c_master_service_t *service,
    uint8_t address, const uint8_t *tx, size_t nt, uint8_t *rx, size_t nr, uint32_t ms)
{
    if (service->io.transfer)
        return service->io.transfer(service->io.context, address, tx, nt, rx, nr, ms);
    return bvstk_i2c_master_hw_transfer_once_locked(service->hardware, address, tx, nt, rx, nr, ms);
}

static void switch_state(bvstk_i2c_master_service_t *service, size_t id,
                         uint8_t value, int known)
{
    bvstk_cache_sink_enter(&service->cache->sink);
    service->switch_value[id] = value;
    service->switch_known[id] = (uint8_t)known;
    bvstk_cache_sink_leave(&service->cache->sink);
}

static bvstk_status_t register_transfer(bvstk_i2c_master_service_t *service,
    const bvstk_i2c_device_t *device, uint8_t reg, uint8_t *value, int write, uint32_t ms)
{
    bvstk_i2c_transfer_t t = bvstk_i2c_transfer_options(&device->endpoint);
    const bvstk_address_space_t *space = bvstk_i2c_register_space(&device->endpoint, reg);
    uint16_t address = t.address_base + reg - (space ? space->first : 0U);
    uint8_t target = space ? space->address : device->addr_7b;
    uint8_t tx[3];
    bvstk_status_t status;
    for (unsigned i = 0; i < t.address_bytes; ++i)
        tx[i] = (uint8_t)(address >> (8U * (t.little_endian ? i : t.address_bytes - 1U - i)));
    tx[t.address_bytes] = *value;
    if (!write && t.stop_before_read && t.address_bytes) {
        status = raw_transfer(service, target, tx, t.address_bytes, NULL, 0, ms);
        if (status != BVSTK_OK) return status;
        return raw_transfer(service, target, NULL, 0, value, 1, ms);
    }
    status = raw_transfer(service, target, tx, t.address_bytes + (write ? 1U : 0U),
        write ? NULL : value, write ? 0U : 1U, ms);
    if (write && status == BVSTK_OK) {
        if (t.write_cycle_ms)
            bvstk_clock_sleep_ms(service->hardware ? &service->hardware->clock : NULL, t.write_cycle_ms);
        if (t.verify_readback) {
            uint8_t verified = 0;
            status = register_transfer(service, device, reg, &verified, 0, ms);
            if (status == BVSTK_OK && verified != *value) status = BVSTK_ERR_IO;
        }
    }
    return status;
}

/* Internal selection/restoration is real I/O and belongs to the switch's
 * telemetry, independently of the downstream endpoint's transaction. */
static bvstk_status_t route_control(bvstk_i2c_master_service_t *service, size_t id,
    uint8_t *value, int write, int restore, uint32_t ms, int quiet)
{
    const bvstk_i2c_device_t *device = &service->devices->items[id];
    uint8_t reg = device->endpoint.router.reg;
    int was_present = presence_confirmed(service, id, reg), permitted;
    if (write) {
        if (!quiet) {
            emit(service, BVSTK_EVENT_REG_ATTEMPT, 0, BVSTK_EVENT_SOURCE_INTERNAL,
                BVSTK_EVENT_OP_WRITE, device, reg, *value);
        }
        bvstk_cache_sink_enter(&service->cache->sink);
        permitted = restore || bvstk_i2c_policy_permits(service->policy, id, device, reg, *value);
        bvstk_cache_sink_leave(&service->cache->sink);
        if (!permitted) {
            if (!quiet) {
                emit(service, BVSTK_EVENT_REG_DENIED, BVSTK_ERR_DENIED, BVSTK_EVENT_SOURCE_INTERNAL,
                    BVSTK_EVENT_OP_WRITE, device, reg, *value);
            }
            return BVSTK_ERR_DENIED;
        }
    }
    bvstk_status_t status = register_transfer(service, device, reg, value, write, ms);
    record_response(service, id, device->addr_7b, status, !write);
    emit_flags(service, write ? BVSTK_EVENT_MASTER_WRITE : BVSTK_EVENT_MASTER_READ,
        (uint16_t)status, BVSTK_EVENT_SOURCE_INTERNAL, write ? BVSTK_EVENT_OP_WRITE : BVSTK_EVENT_OP_READ,
        device, reg, *value, (was_present || status == BVSTK_OK) ?
            quiet ? BVSTK_EVENT_FLAG_TELEMETRY_UNCONFIRMED : BVSTK_EVENT_FLAG_NONE :
            BVSTK_EVENT_FLAG_TELEMETRY_UNCONFIRMED);
    cache_response(service, id, device, reg, *value,
        write ? BVSTK_ERR_NOT_READY : status);
    if (write && !quiet)
        emit(service, status == BVSTK_OK ? BVSTK_EVENT_REG_COMMIT : BVSTK_EVENT_FAULT,
            (uint16_t)status, BVSTK_EVENT_SOURCE_INTERNAL, BVSTK_EVENT_OP_WRITE, device, reg, *value);
    return status;
}

static bvstk_status_t routed_transfer(bvstk_i2c_master_service_t *service,
    const bvstk_i2c_device_t *device, uint8_t reg, uint8_t *value,
    int write, uint32_t ms, int *performed, bvstk_status_t *target_status,
    int quiet)
{
    size_t id, route[BVSTK_I2C_ROUTE_MAX], depth, opened = 0;
    uint8_t channels[BVSTK_I2C_ROUTE_MAX], saved[BVSTK_I2C_ROUTE_MAX];
    bvstk_status_t status, cleanup = BVSTK_OK;
    if (bvstk_i2c_devices_find_by_name(service->devices, device->name, &id) != BVSTK_OK ||
        bvstk_i2c_devices_route(service->devices, id, route, channels, &depth) != BVSTK_OK)
        return BVSTK_ERR_MALFORMED;
    if (service->io.transfer) {
        if (!service->io.lock || !service->io.unlock) return BVSTK_ERR_UNSUPPORTED;
        status = service->io.lock(service->io.context, ms);
    } else status = bvstk_i2c_master_hw_lock(service->hardware, ms);
    if (status != BVSTK_OK) return status;
    for (size_t i = 0; i < depth; ++i) {
        const bvstk_i2c_device_t *sw = &service->devices->items[route[i]];
        const bvstk_i2c_switch_t *router = &sw->endpoint.router;
        uint8_t select;
        saved[i] = 0;
        status = route_control(service, route[i], &saved[i], 0, 0, ms, quiet);
        if (status != BVSTK_OK) { switch_state(service, route[i], 0, 0); break; }
        switch_state(service, route[i], saved[i], 1);
        saved[i] &= router->select_mask | router->preserve_mask;
        select = (saved[i] & router->preserve_mask) | router->channels[channels[i]].select;
        /* Include a possibly partially completed selection in cleanup. */
        opened = i + 1U;
        status = route_control(service, route[i], &select, 1, 0, ms, quiet);
        if (status == BVSTK_ERR_DENIED) opened = i;
        if (status != BVSTK_OK) break;
    }
    if (status == BVSTK_OK) {
        *performed = 1;
        status = register_transfer(service, device, reg, value, write, ms);
        *target_status = status;
        if (device->endpoint.router.channel_count && reg == device->endpoint.router.reg)
            switch_state(service, id, *value, status == BVSTK_OK);
    }
    while (opened) {
        size_t i = --opened;
        bvstk_status_t restored = route_control(service, route[i], &saved[i], 1, 1, ms, quiet);
        switch_state(service, route[i], saved[i], restored == BVSTK_OK);
        if (restored != BVSTK_OK) cleanup = restored;
    }
    if (service->io.transfer) service->io.unlock(service->io.context);
    else bvstk_i2c_master_hw_unlock(service->hardware);
    return cleanup != BVSTK_OK ? cleanup : status;
}

static bvstk_status_t backend_read(bvstk_i2c_master_service_t *service,
                                   const bvstk_i2c_device_t *device,
                                   uint8_t reg,
                                   uint8_t *value,
                                   uint32_t timeout_ms, int *performed,
                                   bvstk_status_t *target_status,
                                   int quiet)
{
    bvstk_status_t status;
    *performed = 0;
    *target_status = BVSTK_ERR_NOT_READY;
    if (device->endpoint.parent[0] || device->endpoint.transfer.configured ||
        (service->io.transfer && !service->io.read_reg)) {
        *value = 0;
        return routed_transfer(service, device, reg, value, 0, timeout_ms, performed, target_status, quiet);
    }
    if (service->io.read_reg != NULL) {
        *performed = 1;
        status = service->io.read_reg(service->io.context,
                                    device->addr_7b,
                                    reg,
                                    value,
                                    timeout_ms);
        *target_status = status;
        return status;
    }
    if (service->hardware == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    *performed = 1;
    status = bvstk_i2c_master_hw_read_reg(service->hardware,
                                        device->addr_7b,
                                        reg,
                                        value,
                                        timeout_ms);
    *target_status = status;
    return status;
}

void bvstk_i2c_master_service_observe(bvstk_i2c_master_service_t *service,
    size_t id, uint16_t type, bvstk_event_op_t op, uint32_t reg, uint8_t value,
    bvstk_status_t status)
{
    bvstk_i2c_device_t device;
    if (service_status(service, id) == BVSTK_OK &&
        bvstk_i2c_devices_get(service->devices, id, &device) == BVSTK_OK)
        emit_flags(service, type, (uint16_t)status, BVSTK_EVENT_SOURCE_HOST, op,
                   &device, reg, value,
                   type == BVSTK_EVENT_SLAVE_REPLY_PREPARED ?
                       BVSTK_EVENT_FLAG_TELEMETRY_UNCONFIRMED :
                       BVSTK_EVENT_FLAG_NONE);
}

void bvstk_i2c_master_service_observe_address(bvstk_i2c_master_service_t *service,
    size_t id, unsigned address, uint16_t type, bvstk_event_op_t op,
    uint32_t reg, uint8_t value, bvstk_status_t status)
{
    bvstk_i2c_device_t device;
    if (service_status(service, id) != BVSTK_OK ||
        bvstk_i2c_devices_get(service->devices, id, &device) != BVSTK_OK ||
        bvstk_i2c_address_space(&device.endpoint, device.addr_7b, address) < 0) return;
    device.addr_7b = (uint8_t)address;
    device.endpoint.space_count = 0;
    emit_flags(service, type, (uint16_t)status, BVSTK_EVENT_SOURCE_HOST, op,
        &device, reg, value, type == BVSTK_EVENT_SLAVE_REPLY_PREPARED ?
        BVSTK_EVENT_FLAG_TELEMETRY_UNCONFIRMED : BVSTK_EVENT_FLAG_NONE);
}

static bvstk_status_t backend_write(bvstk_i2c_master_service_t *service,
                                    const bvstk_i2c_device_t *device,
                                    uint8_t reg,
                                    uint8_t value,
                                    uint32_t timeout_ms, int *performed,
                                    bvstk_status_t *target_status)
{
    bvstk_status_t status;
    *performed = 0;
    *target_status = BVSTK_ERR_NOT_READY;
    if (device->endpoint.parent[0] || device->endpoint.transfer.configured ||
        (service->io.transfer && !service->io.write_reg))
        return routed_transfer(service, device, reg, &value, 1, timeout_ms, performed, target_status, 0);
    if (service->io.write_reg != NULL) {
        *performed = 1;
        status = service->io.write_reg(service->io.context,
                                     device->addr_7b,
                                     reg,
                                     value,
                                     timeout_ms);
        *target_status = status;
        return status;
    }
    if (service->hardware == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    *performed = 1;
    status = bvstk_i2c_master_hw_write_reg(service->hardware,
                                         device->addr_7b,
                                         reg,
                                         value,
                                         timeout_ms);
    *target_status = status;
    return status;
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
         (io == NULL || ((!io->read_reg || !io->write_reg) && !io->transfer)))) {
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
    if (bvstk_i2c_devices_validate_topology(devices) != BVSTK_OK) {
        service->initialized = 0;
        return BVSTK_ERR_MALFORMED;
    }
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

bvstk_status_t bvstk_i2c_master_service_device_presence(
    const bvstk_i2c_master_service_t *service,
    size_t device_id,
    bvstk_i2c_presence_t *out)
{
    bvstk_status_t status = service_status(service, device_id);
    if (status != BVSTK_OK) return status;
    if (out == NULL) return BVSTK_ERR_MALFORMED;
    *out = (bvstk_i2c_presence_t)service->presence[device_id];
    return BVSTK_OK;
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

bvstk_status_t bvstk_i2c_master_service_resolve_host(
    const bvstk_i2c_master_service_t *service, uint8_t address, size_t *out)
{
    size_t found = SIZE_MAX;
    if (!service || !service->initialized || !out) return BVSTK_ERR_NOT_READY;
    for (size_t id = 0; id < service->devices->count; ++id) {
        size_t route[BVSTK_I2C_ROUTE_MAX], depth, i;
        uint8_t channel[BVSTK_I2C_ROUTE_MAX];
        const bvstk_i2c_device_t *d = &service->devices->items[id];
        if (bvstk_i2c_address_space(&d->endpoint, d->addr_7b, address) < 0) continue;
        if (bvstk_i2c_devices_route(service->devices, id, route, channel, &depth) != BVSTK_OK)
            return BVSTK_ERR_MALFORMED;
        for (i = 0; i < depth; ++i)
            if (!service->switch_known[route[i]] ||
                !bvstk_i2c_channel_selected(&service->devices->items[route[i]].endpoint.router,
                    channel[i], service->switch_value[route[i]])) break;
        if (i != depth) continue;
        if (found != SIZE_MAX) return BVSTK_ERR_UNSUPPORTED;
        found = id;
    }
    if (found == SIZE_MAX) return BVSTK_ERR_NOT_FOUND;
    *out = found;
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_master_service_read(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    uint8_t reg,
    uint8_t *value,
    uint32_t timeout_ms)
{
    return bvstk_i2c_master_service_read_from(service, device_id, reg, value,
        BVSTK_EVENT_SOURCE_INTERNAL, timeout_ms);
}

bvstk_status_t bvstk_i2c_master_service_read_from(
    bvstk_i2c_master_service_t *service, size_t device_id, uint8_t reg,
    uint8_t *value, bvstk_event_source_t source, uint32_t timeout_ms)
{
    bvstk_i2c_device_t device;
    int was_present, performed;
    bvstk_status_t target_status;
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
    was_present = presence_confirmed(service, device_id, reg);
    status = backend_read(service, &device, reg, value, timeout_ms, &performed, &target_status, 0);
    /* A route failure is not a transaction with the downstream device.
     * Its telemetry and presence must reflect actual endpoint I/O only. */
    if (performed) {
        record_response(service, device_id, wire_address(&device, reg), target_status, 1);
        emit_flags(service, BVSTK_EVENT_MASTER_READ, (uint16_t)target_status, source,
            BVSTK_EVENT_OP_READ, &device, reg, target_status == BVSTK_OK ? *value : 0,
            (was_present || target_status == BVSTK_OK) ? BVSTK_EVENT_FLAG_NONE :
                BVSTK_EVENT_FLAG_TELEMETRY_UNCONFIRMED);
        cache_response(service, device_id, &device, reg, target_status == BVSTK_OK ? *value : 0, target_status);
    }
    return status;
}

bvstk_status_t bvstk_i2c_master_service_probe(
    bvstk_i2c_master_service_t *service, size_t device_id, uint8_t reg,
    uint32_t timeout_ms)
{
    bvstk_i2c_device_t device;
    int performed;
    bvstk_status_t target_status;
    bvstk_status_t status = service_status(service, device_id);

    if (status != BVSTK_OK) return status;
    if (bvstk_i2c_devices_get(service->devices, device_id, &device) != BVSTK_OK ||
        reg >= device.reg_count) return BVSTK_ERR_RANGE;

    /* A health check is still a real read: presence and the selected cache
     * entry must follow the physical result.  It is marked quiet only so a
     * periodic worker cannot fill the user telemetry ring with its own work. */
    uint8_t value = 0U;
    status = backend_read(service, &device, reg, &value, timeout_ms,
                          &performed, &target_status, 1);
    if (performed) {
        record_response(service, device_id, wire_address(&device, reg),
                        target_status, 1);
        cache_response(service, device_id, &device, reg,
                       target_status == BVSTK_OK ? value : 0U, target_status);
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

bvstk_status_t bvstk_i2c_master_service_cache_entry(
    const bvstk_i2c_master_service_t *service,
    size_t device_id,
    uint8_t reg,
    bvstk_i2c_cache_entry_t *entry)
{
    bvstk_i2c_device_t device;
    bvstk_status_t status = service_status(service, device_id);

    if (status != BVSTK_OK) {
        return status;
    }
    if (entry == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    if (bvstk_i2c_devices_get(service->devices, device_id, &device) != BVSTK_OK ||
        reg >= device.reg_count) {
        return BVSTK_ERR_RANGE;
    }
    return bvstk_i2c_cache_entry(service->cache, device_id, reg, entry);
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
    int was_present, permitted, performed;
    bvstk_status_t target_status;
    bvstk_status_t status = service_status(service, device_id);

    if (status != BVSTK_OK) {
        return status;
    }
    if (bvstk_i2c_devices_get(service->devices, device_id, &device) != BVSTK_OK) {
        return BVSTK_ERR_NOT_FOUND;
    }
    was_present = presence_confirmed(service, device_id, reg);
    emit(service,
         BVSTK_EVENT_REG_ATTEMPT,
         0U,
         source,
         BVSTK_EVENT_OP_WRITE,
         &device,
         reg,
         value);
    if (reg >= device.reg_count || value > device.max_value_code) {
        emit(service, BVSTK_EVENT_FAULT, BVSTK_ERR_RANGE, source,
            BVSTK_EVENT_OP_WRITE, &device, reg, value);
        return BVSTK_ERR_RANGE;
    }
    bvstk_cache_sink_enter(&service->cache->sink);
    permitted = bvstk_i2c_policy_permits(service->policy, device_id,
                                         &device, reg, value);
    if (device.endpoint.read_only[reg / 8U] & (1U << (reg % 8U))) permitted = 0;
    bvstk_cache_sink_leave(&service->cache->sink);
    if (!permitted) {
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
    status = backend_write(service, &device, reg, value, timeout_ms, &performed, &target_status);
    if (performed) {
        record_response(service, device_id, wire_address(&device, reg), target_status, 0);
        emit_flags(service, BVSTK_EVENT_MASTER_WRITE, (uint16_t)target_status, source,
            BVSTK_EVENT_OP_WRITE, &device, reg, value,
            (was_present || target_status == BVSTK_OK) ? BVSTK_EVENT_FLAG_NONE :
                BVSTK_EVENT_FLAG_TELEMETRY_UNCONFIRMED);
    }
    if (status != BVSTK_OK) {
        emit(service,
             BVSTK_EVENT_FAULT,
             (uint16_t)status,
             source,
             BVSTK_EVENT_OP_WRITE,
             &device,
             reg,
             value);
        if (performed) cache_response(service, device_id, &device, reg, 0, status);
        return status;
    }
    cache_response(service, device_id, &device, reg, value,
        device.endpoint.transfer.invalidate_after_write ? BVSTK_ERR_NOT_READY : BVSTK_OK);
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
    if (status != BVSTK_OK) return status;
    bvstk_cache_sink_enter(&service->cache->sink);
    status = bvstk_i2c_policy_set_mode(service->policy, device_id, mode);
    bvstk_cache_sink_leave(&service->cache->sink);
    return status;
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
    bvstk_cache_sink_enter(&service->cache->sink);
    status = bvstk_i2c_policy_get(service->policy, device_id, out);
    bvstk_cache_sink_leave(&service->cache->sink);
    return status;
}

bvstk_status_t bvstk_i2c_master_service_set_config(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    const i2c_device_config_t *config)
{
    bvstk_i2c_device_t device, previous;
    bvstk_status_t status = service_status(service, device_id);

    if (status != BVSTK_OK) {
        return status;
    }
    if (config == NULL || config->name[0] == '\0' || config->reg_count == 0U ||
        config->reg_count > I2C_CFG_MAX_REG_COUNT ||
        config->addr_7b > UINT8_C(0x7F) ||
        config->presence_check_enabled > 1U ||
        (config->presence_check_enabled && config->presence_check_reg >= config->reg_count) ||
        !memchr(config->name, 0, sizeof(config->name)) ||
        bvstk_i2c_endpoint_validate(&config->endpoint, config->reg_count) != BVSTK_OK) {
        return BVSTK_ERR_MALFORMED;
    }
    status = bvstk_i2c_devices_get(service->devices, device_id, &previous);
    if (status != BVSTK_OK) return status;
    /* Rewiring a live topology requires a restart; policies remain editable. */
    for (size_t i = 0; i < service->devices->count; ++i) {
        const bvstk_i2c_device_t *d = &service->devices->items[i];
        if ((d->endpoint.parent[0] || d->endpoint.router.channel_count ||
             d->endpoint.transfer.configured || config->endpoint.transfer.configured) &&
            (previous.addr_7b != config->addr_7b || previous.reg_count != config->reg_count ||
             strcmp(previous.name, config->name) ||
             memcmp(&previous.endpoint, &config->endpoint, sizeof(previous.endpoint))))
            return BVSTK_ERR_UNSUPPORTED;
    }
    memset(&device, 0, sizeof(device));
    memcpy(device.name, config->name, sizeof(device.name));
    device.name[sizeof(device.name) - 1U] = '\0';
    device.addr_7b = config->addr_7b;
    device.reg_count = config->reg_count;
    device.max_value_code = config->max_value_code;
    device.presence_check_enabled = config->presence_check_enabled;
    device.presence_check_reg = config->presence_check_reg;
    device.endpoint = config->endpoint;
    bvstk_cache_sink_enter(&service->cache->sink);
    status = bvstk_i2c_devices_set(service->devices, device_id, &device);
    if (status != BVSTK_OK) {
        bvstk_cache_sink_leave(&service->cache->sink);
        return status;
    }
    if (previous.addr_7b != device.addr_7b || previous.reg_count != device.reg_count) {
        service->presence[device_id] = BVSTK_I2C_PRESENCE_UNKNOWN;
        memset(service->space_presence[device_id], BVSTK_I2C_PRESENCE_UNKNOWN,
               sizeof(service->space_presence[device_id]));
        for (unsigned r = 0; r < I2C_CFG_MAX_REG_COUNT; ++r)
            (void)bvstk_i2c_cache_invalidate(service->cache, device_id, (uint8_t)r);
    }
    status = bvstk_i2c_policy_set_config(service->policy, device_id, config);
    if (status == BVSTK_OK && service->cache_config_changed)
        service->cache_config_changed(service->cache->sink.context, device_id, config);
    bvstk_cache_sink_leave(&service->cache->sink);
    return status;
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
    bvstk_cache_sink_enter(&service->cache->sink);
    status = bvstk_i2c_devices_get(service->devices, device_id, &device);
    if (status != BVSTK_OK) {
        bvstk_cache_sink_leave(&service->cache->sink);
        return status;
    }
    status = bvstk_i2c_policy_get(service->policy, device_id, &policy);
    if (status != BVSTK_OK) {
        bvstk_cache_sink_leave(&service->cache->sink);
        return status;
    }
    memset(out, 0, sizeof(*out));
    memcpy(out->name, device.name, sizeof(out->name));
    out->name[sizeof(out->name) - 1U] = '\0';
    out->addr_7b = device.addr_7b;
    out->reg_count = device.reg_count;
    out->max_value_code = device.max_value_code;
    out->presence_check_enabled = device.presence_check_enabled;
    out->presence_check_reg = device.presence_check_reg;
    out->endpoint = device.endpoint;
    out->policy = policy.mode;
    memcpy(out->whitelist,
           policy.whitelist,
           policy.whitelist_len * sizeof(policy.whitelist[0]));
    out->whitelist_len = policy.whitelist_len;
    memcpy(out->blacklist,
           policy.blacklist,
           policy.blacklist_len * sizeof(policy.blacklist[0]));
    out->blacklist_len = policy.blacklist_len;
    bvstk_cache_sink_leave(&service->cache->sink);
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
    bvstk_cache_sink_enter(&service->cache->sink);
    status = bvstk_i2c_devices_get(service->devices, device_id, &device);
    if (status == BVSTK_OK)
        status = bvstk_i2c_policy_add_allow(service->policy, device_id,
                                            reg, value, &device);
    bvstk_cache_sink_leave(&service->cache->sink);
    return status;
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
    bvstk_cache_sink_enter(&service->cache->sink);
    status = bvstk_i2c_devices_get(service->devices, device_id, &device);
    if (status == BVSTK_OK)
        status = bvstk_i2c_policy_add_deny(service->policy, device_id,
                                           reg, value, &device);
    bvstk_cache_sink_leave(&service->cache->sink);
    return status;
}

bvstk_status_t bvstk_i2c_master_service_rule_clear(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    uint8_t reg,
    uint8_t value)
{
    bvstk_status_t status = service_status(service, device_id);
    if (status != BVSTK_OK) return status;
    bvstk_cache_sink_enter(&service->cache->sink);
    status = bvstk_i2c_policy_remove(service->policy, device_id, reg, value);
    bvstk_cache_sink_leave(&service->cache->sink);
    return status;
}

bvstk_status_t bvstk_i2c_master_service_rule_remove(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    i2c_policy_t list,
    uint8_t reg,
    uint8_t value)
{
    bvstk_i2c_device_t device;
    bvstk_status_t status = service_status(service, device_id);
    if (status != BVSTK_OK) return status;
    if (list != I2C_POLICY_WHITELIST && list != I2C_POLICY_BLACKLIST)
        return BVSTK_ERR_MALFORMED;
    bvstk_cache_sink_enter(&service->cache->sink);
    status = bvstk_i2c_devices_get(service->devices, device_id, &device);
    if (status == BVSTK_OK) {
        if (reg >= device.reg_count || value > device.max_value_code)
            status = BVSTK_ERR_RANGE;
        else status = bvstk_i2c_policy_remove_list(service->policy, device_id,
                                                   list, reg, value);
    }
    bvstk_cache_sink_leave(&service->cache->sink);
    return status;
}

bvstk_status_t bvstk_i2c_master_service_rule_clear_list(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    i2c_policy_t list)
{
    bvstk_status_t status = service_status(service, device_id);
    if (status != BVSTK_OK) return status;
    bvstk_cache_sink_enter(&service->cache->sink);
    status = bvstk_i2c_policy_clear(service->policy, device_id, list);
    bvstk_cache_sink_leave(&service->cache->sink);
    return status;
}

bvstk_status_t bvstk_i2c_master_service_rule_clear_range(
    bvstk_i2c_master_service_t *service, size_t id, i2c_policy_t list,
    unsigned first, unsigned count)
{
    bvstk_status_t st = service_status(service, id);
    if (st != BVSTK_OK) return st;
    if ((list != I2C_POLICY_WHITELIST && list != I2C_POLICY_BLACKLIST) ||
        !count || first >= I2C_CFG_MAX_REG_COUNT || count > I2C_CFG_MAX_REG_COUNT - first)
        return BVSTK_ERR_RANGE;
    bvstk_cache_sink_enter(&service->cache->sink);
    bvstk_i2c_policy_entry_t *entry = &service->policy->items[id];
    i2c_rule_entry_t *rules = list == I2C_POLICY_WHITELIST ? entry->whitelist : entry->blacklist;
    size_t *length = list == I2C_POLICY_WHITELIST ? &entry->whitelist_len : &entry->blacklist_len;
    size_t kept = 0;
    for (size_t i = 0; i < *length; ++i)
        if (rules[i].reg < first || rules[i].reg - first >= count) rules[kept++] = rules[i];
    memset(rules + kept, 0, (*length - kept) * sizeof(*rules));
    *length = kept;
    bvstk_cache_sink_leave(&service->cache->sink);
    return BVSTK_OK;
}
