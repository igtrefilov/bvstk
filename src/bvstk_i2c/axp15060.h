#ifndef I2C_DEVICE_PROFILE_AXP15060_H
#define I2C_DEVICE_PROFILE_AXP15060_H

#define I2CDEV_I2C_ADDR_7B        0x36u
#define I2CDEV_REG_COUNT          0x4Au
#define I2CDEV_MAX_VALUE_CODE     0x81u
#define I2CDEV_DEFAULT_POLICY     I2CDEV_POLICY_WHITELIST

static const i2cdev_rule_entry_t i2cdev_default_whitelist[] = {
    { 0x13u, 16u },
    { 0x13u, 17u },
    { 0x13u, 18u },
    { 0x13u, 19u },
};
static const size_t i2cdev_default_whitelist_len = sizeof(i2cdev_default_whitelist) / sizeof(i2cdev_default_whitelist[0]);

static const i2cdev_rule_entry_t i2cdev_default_blacklist[] = { };
static const size_t i2cdev_default_blacklist_len = 0u;

static const uint8_t i2cdev_autopoll_regs[] = { 0x00u, 0x31u, 0x32u, 0x48u, 0x49u };

static const i2cdev_autopoll_profile_t i2cdev_autopoll_profile = {
    .regs = i2cdev_autopoll_regs,
    .regs_len = sizeof(i2cdev_autopoll_regs) / sizeof(i2cdev_autopoll_regs[0]),
    .reg_delay_ms = 5u,
    .cycle_delay_ms = 200u,
    .enabled = true,
};

#define I2CDEV_DEFAULT_RULES_DEFINED 1
#define I2CDEV_AUTOPOLL_PROFILE_DEFINED 1

#endif
