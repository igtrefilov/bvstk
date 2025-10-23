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

static const uint8_t i2cdev_whitelist_bitmap[I2CDEV_REG_COUNT][I2CDEV_MAX_VALUE_CODE + 1] = {
    [0x13] = {
        [16] = 1, [17] = 1, [18] = 1, [19] = 1
    }
};

static const uint8_t i2cdev_blacklist_bitmap[I2CDEV_REG_COUNT][I2CDEV_MAX_VALUE_CODE + 1] = {
};

static inline bool i2cdev_is_value_permitted(i2cdev_policy_t policy, uint8_t reg, uint8_t val)
{
    if (reg >= I2CDEV_REG_COUNT) return false;
    if (val > I2CDEV_MAX_VALUE_CODE) return false;
    if (policy == I2CDEV_POLICY_WHITELIST) {
        return i2cdev_whitelist_bitmap[reg][val] != 0;
    } else {
        return i2cdev_blacklist_bitmap[reg][val] == 0;
    }
}

#endif
