#include "services/i2c/bvstk_i2c_policy.h"

#include <string.h>

static bvstk_status_t policy_status(const bvstk_i2c_policy_t *policy,
                                    size_t device_id)
{
    if (policy == NULL || policy->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    return device_id < policy->device_count ? BVSTK_OK : BVSTK_ERR_NOT_FOUND;
}

static int rule_contains(const i2c_rule_entry_t *rules,
                         size_t length,
                         uint8_t reg,
                         uint8_t value)
{
    size_t i;
    for (i = 0U; i < length; ++i) {
        if (rules[i].reg == reg && rules[i].val == value) {
            return 1;
        }
    }
    return 0;
}

static int mode_valid(i2c_policy_t mode)
{
    return mode == I2C_POLICY_WHITELIST || mode == I2C_POLICY_BLACKLIST;
}

static int rule_valid(const bvstk_i2c_device_t *device,
                      uint8_t reg,
                      uint8_t value)
{
    return device != NULL && reg < device->reg_count &&
           value <= device->max_value_code;
}

static void add_rule(i2c_rule_entry_t *rules,
                     size_t *length,
                     uint8_t reg,
                     uint8_t value)
{
    if (!rule_contains(rules, *length, reg, value) &&
        *length < I2C_CFG_RULES_MAX) {
        rules[*length].reg = reg;
        rules[*length].val = value;
        (*length)++;
    }
}

static void remove_rule(i2c_rule_entry_t *rules,
                        size_t *length,
                        uint8_t reg,
                        uint8_t value)
{
    size_t i;
    size_t write_index = 0U;
    for (i = 0U; i < *length; ++i) {
        if (rules[i].reg == reg && rules[i].val == value) {
            continue;
        }
        rules[write_index++] = rules[i];
    }
    *length = write_index;
}

bvstk_status_t bvstk_i2c_policy_init(bvstk_i2c_policy_t *policy,
                                     const i2c_device_config_t *configs,
                                     size_t device_count)
{
    size_t i;

    if (policy == NULL || device_count > I2C_CFG_MAX_DEVICES ||
        (device_count != 0U && configs == NULL)) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(policy, 0, sizeof(*policy));
    for (i = 0U; i < device_count; ++i) {
        if (configs[i].reg_count == 0U ||
            configs[i].reg_count > I2C_CFG_MAX_REG_COUNT ||
            !mode_valid(configs[i].policy) ||
            configs[i].whitelist_len > I2C_CFG_RULES_MAX ||
            configs[i].blacklist_len > I2C_CFG_RULES_MAX) {
            return BVSTK_ERR_MALFORMED;
        }
        policy->items[i].mode = configs[i].policy;
        memcpy(policy->items[i].whitelist,
               configs[i].whitelist,
               configs[i].whitelist_len * sizeof(configs[i].whitelist[0]));
        policy->items[i].whitelist_len = configs[i].whitelist_len;
        memcpy(policy->items[i].blacklist,
               configs[i].blacklist,
               configs[i].blacklist_len * sizeof(configs[i].blacklist[0]));
        policy->items[i].blacklist_len = configs[i].blacklist_len;
    }
    policy->device_count = device_count;
    policy->initialized = 1U;
    return BVSTK_OK;
}

void bvstk_i2c_policy_shutdown(bvstk_i2c_policy_t *policy)
{
    if (policy != NULL) {
        memset(policy, 0, sizeof(*policy));
    }
}

bvstk_status_t bvstk_i2c_policy_get(const bvstk_i2c_policy_t *policy,
                                    size_t device_id,
                                    bvstk_i2c_policy_entry_t *out)
{
    bvstk_status_t status = policy_status(policy, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (out == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    *out = policy->items[device_id];
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_policy_set_config(
    bvstk_i2c_policy_t *policy,
    size_t device_id,
    const i2c_device_config_t *config)
{
    bvstk_status_t status = policy_status(policy, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (config == NULL || !mode_valid(config->policy) ||
        config->whitelist_len > I2C_CFG_RULES_MAX ||
        config->blacklist_len > I2C_CFG_RULES_MAX) {
        return BVSTK_ERR_MALFORMED;
    }
    policy->items[device_id].mode = config->policy;
    memcpy(policy->items[device_id].whitelist,
           config->whitelist,
           config->whitelist_len * sizeof(config->whitelist[0]));
    policy->items[device_id].whitelist_len = config->whitelist_len;
    memcpy(policy->items[device_id].blacklist,
           config->blacklist,
           config->blacklist_len * sizeof(config->blacklist[0]));
    policy->items[device_id].blacklist_len = config->blacklist_len;
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_policy_set_mode(bvstk_i2c_policy_t *policy,
                                         size_t device_id,
                                         i2c_policy_t mode)
{
    bvstk_status_t status = policy_status(policy, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (!mode_valid(mode)) {
        return BVSTK_ERR_MALFORMED;
    }
    policy->items[device_id].mode = mode;
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_policy_add_allow(bvstk_i2c_policy_t *policy,
                                          size_t device_id,
                                          uint8_t reg,
                                          uint8_t value,
                                          const bvstk_i2c_device_t *device)
{
    bvstk_status_t status = policy_status(policy, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (!rule_valid(device, reg, value)) {
        return BVSTK_ERR_RANGE;
    }
    add_rule(policy->items[device_id].whitelist,
             &policy->items[device_id].whitelist_len,
             reg,
             value);
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_policy_add_deny(bvstk_i2c_policy_t *policy,
                                         size_t device_id,
                                         uint8_t reg,
                                         uint8_t value,
                                         const bvstk_i2c_device_t *device)
{
    bvstk_status_t status = policy_status(policy, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (!rule_valid(device, reg, value)) {
        return BVSTK_ERR_RANGE;
    }
    add_rule(policy->items[device_id].blacklist,
             &policy->items[device_id].blacklist_len,
             reg,
             value);
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_policy_remove(bvstk_i2c_policy_t *policy,
                                       size_t device_id,
                                       uint8_t reg,
                                       uint8_t value)
{
    bvstk_status_t status = policy_status(policy, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    remove_rule(policy->items[device_id].whitelist,
                &policy->items[device_id].whitelist_len,
                reg,
                value);
    remove_rule(policy->items[device_id].blacklist,
                &policy->items[device_id].blacklist_len,
                reg,
                value);
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_policy_clear(bvstk_i2c_policy_t *policy,
                                      size_t device_id,
                                      i2c_policy_t list)
{
    bvstk_status_t status = policy_status(policy, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (list == I2C_POLICY_WHITELIST) {
        policy->items[device_id].whitelist_len = 0U;
    } else if (list == I2C_POLICY_BLACKLIST) {
        policy->items[device_id].blacklist_len = 0U;
    } else {
        return BVSTK_ERR_MALFORMED;
    }
    return BVSTK_OK;
}

int bvstk_i2c_policy_permits(const bvstk_i2c_policy_t *policy,
                             size_t device_id,
                             const bvstk_i2c_device_t *device,
                             uint8_t reg,
                             uint8_t value)
{
    const bvstk_i2c_policy_entry_t *entry;

    if (policy_status(policy, device_id) != BVSTK_OK ||
        !rule_valid(device, reg, value)) {
        return 0;
    }
    entry = &policy->items[device_id];
    if (entry->mode == I2C_POLICY_WHITELIST) {
        return rule_contains(entry->whitelist,
                             entry->whitelist_len,
                             reg,
                             value);
    }
    return !rule_contains(entry->blacklist,
                          entry->blacklist_len,
                          reg,
                          value);
}
