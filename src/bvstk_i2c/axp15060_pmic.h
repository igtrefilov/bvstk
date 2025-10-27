#ifndef I2C_DEVICE_PROFILE_H
#define I2C_DEVICE_PROFILE_H

#include <stdint.h>
#include <stdbool.h>

#define I2CDEV_I2C_ADDR_7B        0x36
#define I2CDEV_REG_COUNT          0x4A
#define I2CDEV_MAX_VALUE_CODE     0x81

typedef enum {
    I2CDEV_POLICY_WHITELIST = 0,
    I2CDEV_POLICY_BLACKLIST = 1
} i2cdev_policy_t;

#define I2CDEV_DEFAULT_POLICY I2CDEV_POLICY_WHITELIST

extern uint8_t i2cdev_whitelist_bitmap[I2CDEV_REG_COUNT][I2CDEV_MAX_VALUE_CODE + 1];
extern uint8_t i2cdev_blacklist_bitmap[I2CDEV_REG_COUNT][I2CDEV_MAX_VALUE_CODE + 1];

void i2cdev_policy_reset_defaults(void);
void i2cdev_set_policy(i2cdev_policy_t policy);
i2cdev_policy_t i2cdev_get_policy(void);
bool i2cdev_is_value_permitted_current(uint8_t reg, uint8_t val);
bool i2cdev_rule_allow(uint8_t reg, uint8_t val);
bool i2cdev_rule_deny(uint8_t reg, uint8_t val);
bool i2cdev_rule_clear(uint8_t reg, uint8_t val);

#endif
