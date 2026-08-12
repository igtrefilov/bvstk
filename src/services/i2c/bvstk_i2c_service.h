#ifndef BVSTK_I2C_SERVICE_H
#define BVSTK_I2C_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "drivers/pl/i2c/bvstk_i2c_core.h"
#include "shared/config/bvstk_config_model.h"
#include "shared/events/bvstk_event.h"

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
                                bvstk_event_source_t source,
                                uint32_t timeout_ms);
} bvstk_i2c_bus_ops_t;

typedef struct {
    const char *name;
    uint8_t addr_7b;
    uint16_t reg_count;
    uint8_t max_value_code;
} bvstk_i2c_device_info_t;

typedef struct {
    bvstk_i2c_core_t *core;
    bvstk_i2c_bus_ops_t bus;
    bvstk_event_sink_t events;
    i2c_device_config_t devices[I2C_CFG_MAX_DEVICES];
    uint8_t cache[I2C_CFG_MAX_DEVICES][I2C_CFG_MAX_REG_COUNT];
    size_t device_count;
    size_t selected_device;
    uint8_t initialized;
} bvstk_i2c_service_t;

bvstk_status_t bvstk_i2c_service_init(bvstk_i2c_service_t *service,
                                      bvstk_i2c_core_t *core,
                                      const bvstk_i2c_bus_ops_t *bus,
                                      const i2c_device_config_t *devices,
                                      size_t device_count,
                                      const bvstk_event_sink_t *events);
void bvstk_i2c_service_shutdown(bvstk_i2c_service_t *service);

size_t bvstk_i2c_service_device_count(const bvstk_i2c_service_t *service);
bvstk_status_t bvstk_i2c_service_device_info(const bvstk_i2c_service_t *service,
                                             size_t device_id,
                                             bvstk_i2c_device_info_t *out);
bvstk_status_t bvstk_i2c_service_find_by_name(const bvstk_i2c_service_t *service,
                                              const char *name,
                                              size_t *device_id);
bvstk_status_t bvstk_i2c_service_find_by_addr(const bvstk_i2c_service_t *service,
                                              uint8_t addr_7b,
                                              size_t *device_id);

bvstk_status_t bvstk_i2c_service_read_reg(bvstk_i2c_service_t *service,
                                          size_t device_id,
                                          uint8_t reg,
                                          uint8_t *value,
                                          uint32_t timeout_ms);
bvstk_status_t bvstk_i2c_service_read_cached(const bvstk_i2c_service_t *service,
                                             size_t device_id,
                                             uint8_t reg,
                                             uint8_t *value);
bvstk_status_t bvstk_i2c_service_write_reg(bvstk_i2c_service_t *service,
                                           size_t device_id,
                                           uint8_t reg,
                                           uint8_t value,
                                           bvstk_event_source_t source,
                                           uint32_t timeout_ms);

bvstk_status_t bvstk_i2c_service_set_policy(bvstk_i2c_service_t *service,
                                            size_t device_id,
                                            i2c_policy_t policy);
bvstk_status_t bvstk_i2c_service_get_config(const bvstk_i2c_service_t *service,
                                            size_t device_id,
                                            i2c_device_config_t *out);
bvstk_status_t bvstk_i2c_service_set_config(bvstk_i2c_service_t *service,
                                            size_t device_id,
                                            const i2c_device_config_t *config);

/* Execute one configured autopoll pass. Scheduling remains an OS concern. */
bvstk_status_t bvstk_i2c_service_poll(bvstk_i2c_service_t *service,
                                      uint32_t timeout_ms);

#endif
