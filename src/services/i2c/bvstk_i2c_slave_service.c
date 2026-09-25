#include "services/i2c/bvstk_i2c_slave_service.h"

#include <string.h>

static bvstk_status_t service_status(const bvstk_i2c_slave_service_t *service)
{
    if (service == NULL || service->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (service->target_device >= bvstk_i2c_devices_count(service->devices)) {
        return BVSTK_ERR_NOT_FOUND;
    }
    return BVSTK_OK;
}

static bvstk_status_t load_window(bvstk_i2c_slave_service_t *service,
                                  size_t device_id,
                                  uint8_t reg,
                                  uint8_t *window,
                                  size_t capacity,
                                  size_t *size,
                                  uint32_t timeout_ms)
{
    bvstk_i2c_device_t device;
    size_t count;
    size_t i;
    bvstk_status_t status;

    status = bvstk_i2c_devices_get(service->devices, device_id, &device);
    if (status != BVSTK_OK) {
        return status;
    }
    if (reg >= device.reg_count || window == NULL || size == NULL) {
        return BVSTK_ERR_RANGE;
    }
    count = 0U;
    for (i = 0U;
         i < capacity && (size_t)reg + i < device.reg_count;
         ++i) {
        if (bvstk_i2c_cache_is_valid(
                service->cache,
                device_id,
                (uint8_t)((size_t)reg + i)) &&
            bvstk_i2c_cache_read(service->cache,
                                 device_id,
                                 (uint8_t)((size_t)reg + i),
                                 &window[i]) == BVSTK_OK) {
            ++count;
            continue;
        }
        /* Do not turn one host read into a burst of slow PHY transactions.
         * Fetch one missing byte; the PL core raises RD_REQUEST again if the
         * external master continues reading past this short window. */
        if (i != 0U) {
            break;
        }
        status = bvstk_i2c_master_service_read_from(service->master,
                                                device_id,
                                                reg,
                                                &window[0],
                                                BVSTK_EVENT_SOURCE_HOST,
                                                timeout_ms);
        if (status != BVSTK_OK) {
            return status;
        }
        count = 1U;
        break;
    }
    *size = count;
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_slave_service_init(
    bvstk_i2c_slave_service_t *service,
    bvstk_i2c_devices_t *devices,
    bvstk_i2c_cache_t *cache,
    bvstk_i2c_master_service_t *master,
    size_t target_device)
{
    if (service == NULL || devices == NULL || cache == NULL || master == NULL ||
        devices->initialized == 0U || cache->initialized == 0U ||
        target_device >= devices->count || master->initialized == 0U) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(service, 0, sizeof(*service));
    service->devices = devices;
    service->cache = cache;
    service->master = master;
    service->target_device = target_device;
    service->initialized = 1U;
    return BVSTK_OK;
}

void bvstk_i2c_slave_service_shutdown(bvstk_i2c_slave_service_t *service)
{
    if (service != NULL) {
        memset(service, 0, sizeof(*service));
    }
}

bvstk_status_t bvstk_i2c_slave_service_set_target(
    bvstk_i2c_slave_service_t *service,
    size_t target_device)
{
    bvstk_status_t status = service_status(service);
    if (status != BVSTK_OK) {
        return status;
    }
    if (target_device >= bvstk_i2c_devices_count(service->devices)) {
        return BVSTK_ERR_NOT_FOUND;
    }
    service->target_device = target_device;
    return BVSTK_OK;
}

size_t bvstk_i2c_slave_service_target(
    const bvstk_i2c_slave_service_t *service)
{
    return service != NULL && service->initialized != 0U
               ? service->target_device
               : 0U;
}

void bvstk_i2c_slave_service_end_transaction(
    bvstk_i2c_slave_service_t *service)
{
    if (service_status(service) == BVSTK_OK) {
        service->read_armed[service->target_device] = 0U;
    }
}

bvstk_status_t bvstk_i2c_slave_service_handle_frame(
    bvstk_i2c_slave_service_t *service,
    const uint8_t *frame,
    size_t frame_size,
    uint8_t read_phase,
    uint8_t *read_window,
    size_t read_window_capacity,
    size_t *read_window_size,
    uint32_t timeout_ms)
{
    bvstk_i2c_device_t device;
    size_t target;
    size_t reg;
    size_t count;
    size_t i;
    bvstk_status_t status;

    status = service_status(service);
    if (status != BVSTK_OK) {
        return status;
    }
    if (read_window_size == NULL ||
        (read_window_capacity != 0U && read_window == NULL)) {
        return BVSTK_ERR_MALFORMED;
    }
    *read_window_size = 0U;
    target = service->target_device;
    status = bvstk_i2c_devices_get(service->devices, target, &device);
    if (status != BVSTK_OK) {
        return status;
    }

    if (read_phase != 0U) {
        reg = service->register_pointer[target];
        bvstk_i2c_master_service_observe(service->master, target,
            BVSTK_EVENT_SLAVE_READ_REQUEST, BVSTK_EVENT_OP_READ,
            (uint8_t)reg, 0, BVSTK_OK);
        if (service->read_armed[target] == 0U) {
            return BVSTK_OK;
        }
        if (reg >= device.reg_count) {
            reg = 0U;
        }
        status = load_window(service,
                             target,
                             (uint8_t)reg,
                             read_window,
                             read_window_capacity,
                             read_window_size,
                             timeout_ms);
        if (status != BVSTK_OK) {
            return status;
        }
        count = *read_window_size;
        for (i = 0; i < count; ++i)
            bvstk_i2c_master_service_observe(service->master, target,
                BVSTK_EVENT_SLAVE_REPLY_PREPARED, BVSTK_EVENT_OP_READ,
                (uint8_t)(reg + i), read_window[i], BVSTK_OK);
        service->register_pointer[target] =
            (uint8_t)((count != 0U && reg + count < device.reg_count) ?
                          reg + count : 0U);
        return BVSTK_OK;
    }

    /* A new write phase replaces the previous pointer. Invalid frames and
     * rejected writes must never leave an older read armed. */
    service->read_armed[target] = 0U;
    if (frame == NULL || frame_size == 0U) {
        return BVSTK_ERR_MALFORMED;
    }
    reg = frame[0];
    bvstk_i2c_master_service_observe(service->master, target,
        BVSTK_EVENT_SLAVE_POINTER, BVSTK_EVENT_OP_WRITE, (uint8_t)reg, frame[0], BVSTK_OK);
    for (i = 1U; i < frame_size; ++i)
        bvstk_i2c_master_service_observe(service->master, target,
            BVSTK_EVENT_CAPTURE_WRITE, BVSTK_EVENT_OP_WRITE,
            (uint32_t)(reg + i - 1U), frame[i], BVSTK_OK);
    if (reg >= device.reg_count) {
        for (i = 1; i < frame_size; ++i) {
            bvstk_i2c_master_service_observe(service->master, target,
                BVSTK_EVENT_REG_ATTEMPT, BVSTK_EVENT_OP_WRITE,
                (uint32_t)(reg + i - 1U), frame[i], BVSTK_OK);
            bvstk_i2c_master_service_observe(service->master, target,
                BVSTK_EVENT_FAULT, BVSTK_EVENT_OP_WRITE,
                (uint32_t)(reg + i - 1U), frame[i], BVSTK_ERR_RANGE);
        }
        return BVSTK_ERR_RANGE;
    }
    service->register_pointer[target] = (uint8_t)reg;
    service->read_armed[target] = frame_size == 1U ? 1U : 0U;
    for (i = 1U; i < frame_size; ++i) {
        uint8_t current_reg = (uint8_t)(reg + i - 1U);
        if (reg + i - 1U >= device.reg_count) {
            for (; i < frame_size; ++i) {
                bvstk_i2c_master_service_observe(service->master, target,
                    BVSTK_EVENT_REG_ATTEMPT, BVSTK_EVENT_OP_WRITE,
                    (uint32_t)(reg + i - 1U), frame[i], BVSTK_OK);
                bvstk_i2c_master_service_observe(service->master, target,
                    BVSTK_EVENT_FAULT, BVSTK_EVENT_OP_WRITE,
                    (uint32_t)(reg + i - 1U), frame[i], BVSTK_ERR_RANGE);
            }
            break;
        }
        status = bvstk_i2c_master_service_write(service->master,
                                                target,
                                                current_reg,
                                                frame[i],
                                                BVSTK_EVENT_SOURCE_HOST,
                                                timeout_ms);
        if (status != BVSTK_OK) {
            /* Preserve stop-on-error forwarding, but audit the unprocessed tail. */
            for (++i; i < frame_size; ++i) {
                bvstk_i2c_master_service_observe(service->master, target,
                    BVSTK_EVENT_REG_ATTEMPT, BVSTK_EVENT_OP_WRITE,
                    (uint32_t)(reg + i - 1U), frame[i], BVSTK_OK);
                bvstk_i2c_master_service_observe(service->master, target,
                    BVSTK_EVENT_FAULT, BVSTK_EVENT_OP_WRITE,
                    (uint32_t)(reg + i - 1U), frame[i], BVSTK_ERR_NOT_READY);
            }
            return status;
        }
    }
    return BVSTK_OK;
}
