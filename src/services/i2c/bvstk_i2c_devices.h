#ifndef BVSTK_I2C_DEVICES_H
#define BVSTK_I2C_DEVICES_H

#include <stddef.h>
#include <stdint.h>

#include "shared/base/bvstk_status.h"
#include "shared/config/bvstk_config_model.h"

/* Runtime descriptors contain only bus-facing device identity and limits. */
typedef struct {
    char name[I2C_CFG_NAME_MAX];
    uint8_t addr_7b;
    uint16_t reg_count;
    uint8_t max_value_code;
    uint8_t presence_check_enabled;
    uint8_t presence_check_reg;
    bvstk_i2c_endpoint_t endpoint;
} bvstk_i2c_device_t;

typedef struct {
    bvstk_i2c_device_t items[I2C_CFG_MAX_DEVICES];
    size_t count;
    uint8_t initialized;
} bvstk_i2c_devices_t;

bvstk_status_t bvstk_i2c_devices_init(bvstk_i2c_devices_t *devices,
                                      const bvstk_i2c_device_t *items,
                                      size_t count);
bvstk_status_t bvstk_i2c_devices_init_from_config(
    bvstk_i2c_devices_t *devices,
    const i2c_device_config_t *configs,
    size_t count);
void bvstk_i2c_devices_shutdown(bvstk_i2c_devices_t *devices);

size_t bvstk_i2c_devices_count(const bvstk_i2c_devices_t *devices);
bvstk_status_t bvstk_i2c_devices_get(const bvstk_i2c_devices_t *devices,
                                     size_t device_id,
                                     bvstk_i2c_device_t *out);
bvstk_status_t bvstk_i2c_devices_set(bvstk_i2c_devices_t *devices,
                                     size_t device_id,
                                     const bvstk_i2c_device_t *item);
bvstk_status_t bvstk_i2c_devices_find_by_name(
    const bvstk_i2c_devices_t *devices,
    const char *name,
    size_t *device_id);
bvstk_status_t bvstk_i2c_devices_find_by_addr(
    const bvstk_i2c_devices_t *devices,
    uint8_t addr_7b,
    size_t *device_id);

/* Root-to-leaf switch IDs, excluding the endpoint. Validates parents/cycles. */
bvstk_status_t bvstk_i2c_devices_route(const bvstk_i2c_devices_t *devices,
    size_t device_id, size_t switches[BVSTK_I2C_ROUTE_MAX],
    uint8_t channels[BVSTK_I2C_ROUTE_MAX], size_t *count);
bvstk_status_t bvstk_i2c_devices_validate_topology(const bvstk_i2c_devices_t *devices);
/* All distinct slave addresses; the PL address filter has 128 bits. */
size_t bvstk_i2c_devices_addresses(const bvstk_i2c_devices_t *, uint8_t out[128]);

/* Pure protocol helpers. No device model names or I/O. */
static inline size_t bvstk_i2c_next_register(bvstk_i2c_sequence_t sequence,
    size_t count, size_t start, size_t offset)
{
    size_t span = sequence.span ? sequence.span : count;
    if (sequence.mode == BVSTK_I2C_SEQUENCE_FIXED) return start;
    if (sequence.mode == BVSTK_I2C_SEQUENCE_WRAP)
        return start / span * span + (start % span + offset) % span;
    return start + offset;
}
static inline int bvstk_i2c_channel_selected(const bvstk_i2c_switch_t *router,
    unsigned channel, uint8_t value)
{
    if (channel >= router->channel_count) return 0;
    const bvstk_i2c_channel_t *c = &router->channels[channel];
    return (value & c->match_mask) == (c->select & c->match_mask);
}

#endif /* BVSTK_I2C_DEVICES_H */
