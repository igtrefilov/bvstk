#ifndef AXP15060_PMIC_H
#define AXP15060_PMIC_H

#include <stdint.h>

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

void axp15060_init_register_allowlist(void);
void axp15060_init_value_allowlist(void);

#endif
