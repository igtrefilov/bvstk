#ifndef BVSTK_I2C_MASTER_SERVICE_H
#define BVSTK_I2C_MASTER_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "drivers/pl/i2c/bvstk_i2c_master.h"
#include "services/i2c/bvstk_i2c_cache.h"
#include "services/i2c/bvstk_i2c_devices.h"
#include "services/i2c/bvstk_i2c_policy.h"
#include "shared/events/bvstk_event.h"

/* Optional test/board backend. Production targets use master_hw directly. */
typedef struct {
    void *context;
    bvstk_status_t (*read_reg)(void *context,
                               uint8_t addr_7b,
                               uint8_t reg,
                               uint8_t *value,
                               uint32_t timeout_ms);
    bvstk_status_t (*write_reg)(void *context,
                                uint8_t addr_7b,
                                uint8_t reg,
                                uint8_t value,
                                uint32_t timeout_ms);
} bvstk_i2c_master_io_t;

typedef struct {
    bvstk_i2c_master_hw_t *hardware;
    bvstk_i2c_master_io_t io;
    bvstk_i2c_devices_t *devices;
    bvstk_i2c_cache_t *cache;
    bvstk_i2c_policy_t *policy;
    bvstk_event_sink_t events;
    uint8_t initialized;
} bvstk_i2c_master_service_t;

bvstk_status_t bvstk_i2c_master_service_init(
    bvstk_i2c_master_service_t *service,
    bvstk_i2c_master_hw_t *hardware,
    const bvstk_i2c_master_io_t *io,
    bvstk_i2c_devices_t *devices,
    bvstk_i2c_cache_t *cache,
    bvstk_i2c_policy_t *policy,
    const bvstk_event_sink_t *events);
void bvstk_i2c_master_service_shutdown(bvstk_i2c_master_service_t *service);

size_t bvstk_i2c_master_service_device_count(
    const bvstk_i2c_master_service_t *service);
bvstk_status_t bvstk_i2c_master_service_device_info(
    const bvstk_i2c_master_service_t *service,
    size_t device_id,
    bvstk_i2c_device_t *out);
bvstk_status_t bvstk_i2c_master_service_find_by_name(
    const bvstk_i2c_master_service_t *service,
    const char *name,
    size_t *device_id);
bvstk_status_t bvstk_i2c_master_service_find_by_addr(
    const bvstk_i2c_master_service_t *service,
    uint8_t addr_7b,
    size_t *device_id);

bvstk_status_t bvstk_i2c_master_service_read(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    uint8_t reg,
    uint8_t *value,
    uint32_t timeout_ms);
bvstk_status_t bvstk_i2c_master_service_read_cached(
    const bvstk_i2c_master_service_t *service,
    size_t device_id,
    uint8_t reg,
    uint8_t *value);
bvstk_status_t bvstk_i2c_master_service_write(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    uint8_t reg,
    uint8_t value,
    bvstk_event_source_t source,
    uint32_t timeout_ms);

bvstk_status_t bvstk_i2c_master_service_set_policy(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    i2c_policy_t mode);
bvstk_status_t bvstk_i2c_master_service_get_policy(
    const bvstk_i2c_master_service_t *service,
    size_t device_id,
    bvstk_i2c_policy_entry_t *out);
bvstk_status_t bvstk_i2c_master_service_set_config(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    const i2c_device_config_t *config);
bvstk_status_t bvstk_i2c_master_service_get_config(
    const bvstk_i2c_master_service_t *service,
    size_t device_id,
    i2c_device_config_t *out);
bvstk_status_t bvstk_i2c_master_service_rule_allow(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    uint8_t reg,
    uint8_t value);
bvstk_status_t bvstk_i2c_master_service_rule_deny(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    uint8_t reg,
    uint8_t value);
bvstk_status_t bvstk_i2c_master_service_rule_clear(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    uint8_t reg,
    uint8_t value);
bvstk_status_t bvstk_i2c_master_service_rule_clear_list(
    bvstk_i2c_master_service_t *service,
    size_t device_id,
    i2c_policy_t list);

#endif /* BVSTK_I2C_MASTER_SERVICE_H */
