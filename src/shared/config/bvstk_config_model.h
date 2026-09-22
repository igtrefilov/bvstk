#ifndef BVSTK_SHARED_CONFIG_MODEL_H
#define BVSTK_SHARED_CONFIG_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t mac[6];
    uint32_t ip_be;
    uint32_t netmask_be;
    uint32_t gateway_be;
    bool has_ip;
    bool has_netmask;
    bool has_gateway;
    bool has_mac;
} network_config_t;

#define I2C_CFG_MAX_DEVICES 32u
#define I2C_CFG_NAME_MAX 32u
#define I2C_CFG_FILE_NAME_MAX 48u
#define I2C_CFG_MAX_REG_COUNT 256u
#define I2C_CFG_RULES_MAX 256u
#define I2C_CFG_SETTINGS_MAX 256u

typedef enum {
    I2C_POLICY_WHITELIST = 0,
    I2C_POLICY_BLACKLIST = 1
} i2c_policy_t;

typedef struct {
    uint8_t reg;
    uint8_t val;
} i2c_rule_entry_t;

typedef struct {
    char name[I2C_CFG_NAME_MAX];
    char file_name[I2C_CFG_FILE_NAME_MAX];
    uint8_t addr_7b;
    uint16_t reg_count;
    uint8_t max_value_code;
    i2c_policy_t policy;
    i2c_rule_entry_t whitelist[I2C_CFG_RULES_MAX];
    size_t whitelist_len;
    i2c_rule_entry_t blacklist[I2C_CFG_RULES_MAX];
    size_t blacklist_len;
    i2c_rule_entry_t settings[I2C_CFG_SETTINGS_MAX];
    size_t settings_len;
} i2c_device_config_t;

#define MDIO_CFG_MAX_DEVICES 8u
#define MDIO_CFG_NAME_MAX 32u
#define MDIO_CFG_FILE_NAME_MAX 48u
#define MDIO_CFG_AUTOPOLL_REGS_MAX 32u
#define MDIO_CFG_RULES_MAX 64u
#define MDIO_CFG_SETTINGS_MAX 32u

typedef enum {
    MDIO_POLICY_WHITELIST = 0,
    MDIO_POLICY_BLACKLIST = 1
} mdio_policy_t;

typedef struct {
    uint8_t reg;
    uint16_t val;
} mdio_setting_entry_t;

/* mask=0xffff matches an exact value; mask=0 matches any value of reg. */
typedef struct {
    uint8_t reg;
    uint16_t val;
    uint16_t mask;
} mdio_rule_entry_t;

typedef struct {
    char name[MDIO_CFG_NAME_MAX];
    char file_name[MDIO_CFG_FILE_NAME_MAX];
    uint8_t phy_addr;
    uint8_t reg_count;
    mdio_policy_t policy;
    bool autopoll_enabled;
    uint8_t autopoll_regs[MDIO_CFG_AUTOPOLL_REGS_MAX];
    size_t autopoll_regs_len;
    uint32_t autopoll_reg_delay_ms;
    uint32_t autopoll_cycle_delay_ms;
    mdio_rule_entry_t whitelist[MDIO_CFG_RULES_MAX];
    size_t whitelist_len;
    mdio_rule_entry_t blacklist[MDIO_CFG_RULES_MAX];
    size_t blacklist_len;
    mdio_setting_entry_t settings[MDIO_CFG_SETTINGS_MAX];
    size_t settings_len;
} mdio_phy_config_t;

#endif /* BVSTK_SHARED_CONFIG_MODEL_H */
