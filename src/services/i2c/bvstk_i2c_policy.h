#ifndef BVSTK_I2C_POLICY_H
#define BVSTK_I2C_POLICY_H

#include <stddef.h>
#include <stdint.h>

#include "services/i2c/bvstk_i2c_devices.h"

typedef struct {
    i2c_policy_t mode;
    i2c_rule_entry_t whitelist[I2C_CFG_RULES_MAX];
    size_t whitelist_len;
    i2c_rule_entry_t blacklist[I2C_CFG_RULES_MAX];
    size_t blacklist_len;
} bvstk_i2c_policy_entry_t;

typedef struct {
    bvstk_i2c_policy_entry_t items[I2C_CFG_MAX_DEVICES];
    size_t device_count;
    uint8_t initialized;
} bvstk_i2c_policy_t;

bvstk_status_t bvstk_i2c_policy_init(bvstk_i2c_policy_t *policy,
                                     const i2c_device_config_t *configs,
                                     size_t device_count);
void bvstk_i2c_policy_shutdown(bvstk_i2c_policy_t *policy);
bvstk_status_t bvstk_i2c_policy_get(const bvstk_i2c_policy_t *policy,
                                    size_t device_id,
                                    bvstk_i2c_policy_entry_t *out);
bvstk_status_t bvstk_i2c_policy_set_config(
    bvstk_i2c_policy_t *policy,
    size_t device_id,
    const i2c_device_config_t *config);
bvstk_status_t bvstk_i2c_policy_set_mode(bvstk_i2c_policy_t *policy,
                                         size_t device_id,
                                         i2c_policy_t mode);
bvstk_status_t bvstk_i2c_policy_add_allow(bvstk_i2c_policy_t *policy,
                                          size_t device_id,
                                          uint8_t reg,
                                          uint8_t value,
                                          const bvstk_i2c_device_t *device);
bvstk_status_t bvstk_i2c_policy_add_deny(bvstk_i2c_policy_t *policy,
                                         size_t device_id,
                                         uint8_t reg,
                                         uint8_t value,
                                         const bvstk_i2c_device_t *device);
bvstk_status_t bvstk_i2c_policy_remove(bvstk_i2c_policy_t *policy,
                                       size_t device_id,
                                       uint8_t reg,
                                       uint8_t value);
bvstk_status_t bvstk_i2c_policy_clear(bvstk_i2c_policy_t *policy,
                                      size_t device_id,
                                      i2c_policy_t list);
int bvstk_i2c_policy_permits(const bvstk_i2c_policy_t *policy,
                             size_t device_id,
                             const bvstk_i2c_device_t *device,
                             uint8_t reg,
                             uint8_t value);

#endif /* BVSTK_I2C_POLICY_H */
