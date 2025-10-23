#ifndef AXP15060_PMIC_H
#define AXP15060_PMIC_H

#include <stdint.h>
#include <stdbool.h>

#define AXP15060_I2C_ADDR_7B        0x36
#define AXP15060_REG_COUNT          0x4A
#define AXP15060_MAX_VALUE_CODE     0x81

typedef enum {
    AXP15060_REG_POWER_ON_SRC = 0x00,
    AXP15060_REG_DATA_BUF0    = 0x04,
    AXP15060_REG_ONOFF1       = 0x10,
    AXP15060_REG_ONOFF2       = 0x11,
    AXP15060_REG_ONOFF3       = 0x12,
    AXP15060_REG_DCDC1_VOLT   = 0x13,
    AXP15060_REG_DCDC2_VOLT   = 0x14,
    AXP15060_REG_DCDC3_VOLT   = 0x15,
    AXP15060_REG_DCDC4_VOLT   = 0x16,
    AXP15060_REG_DCDC5_VOLT   = 0x17,
    AXP15060_REG_DCDC6_VOLT   = 0x18,
    AXP15060_REG_IRQ_EN1      = 0x40,
    AXP15060_REG_IRQ_EN2      = 0x41,
    AXP15060_REG_IRQ_STS1     = 0x48,
    AXP15060_REG_IRQ_STS2     = 0x49
} axp15060_reg_t;

typedef enum {
    AXP15060_POLICY_WHITELIST = 0,
    AXP15060_POLICY_BLACKLIST = 1
} axp15060_policy_t;

#define AXP15060_DEFAULT_POLICY AXP15060_POLICY_WHITELIST

static const uint8_t axp15060_whitelist_bitmap[AXP15060_REG_COUNT][AXP15060_MAX_VALUE_CODE + 1] = {
    [AXP15060_REG_DCDC1_VOLT] = {
        [16] = 1, [17] = 1, [18] = 1, [19] = 1
    }
};

static const uint8_t axp15060_blacklist_bitmap[AXP15060_REG_COUNT][AXP15060_MAX_VALUE_CODE + 1] = {
};

static inline bool axp15060_is_value_permitted(axp15060_policy_t policy, uint8_t reg, uint8_t val)
{
    if (reg >= AXP15060_REG_COUNT) return false;
    if (val > AXP15060_MAX_VALUE_CODE) return false;
    if (policy == AXP15060_POLICY_WHITELIST) {
        return axp15060_whitelist_bitmap[reg][val] != 0;
    } else {
        return axp15060_blacklist_bitmap[reg][val] == 0;
    }
}

#endif
