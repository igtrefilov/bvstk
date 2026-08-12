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

#define SMI_CFG_MAX_DEVICES 8u
#define SMI_CFG_NAME_MAX 32u
#define SMI_CFG_FILE_NAME_MAX 48u
#define SMI_CFG_AUTOPOLL_REGS_MAX 32u
#define SMI_CFG_WRITE_REGS_MAX 32u
#define SMI_CFG_SETTINGS_MAX 32u

typedef enum {
    SMI_POLICY_WHITELIST = 0,
    SMI_POLICY_BLACKLIST = 1
} smi_policy_t;

typedef struct {
    uint8_t reg;
    uint16_t val;
} smi_setting_entry_t;

typedef struct {
    char name[SMI_CFG_NAME_MAX];
    char file_name[SMI_CFG_FILE_NAME_MAX];
    uint8_t phy_addr;
    uint8_t reg_count;
    smi_policy_t policy;
    bool autopoll_enabled;
    uint8_t autopoll_regs[SMI_CFG_AUTOPOLL_REGS_MAX];
    size_t autopoll_regs_len;
    uint32_t autopoll_reg_delay_ms;
    uint32_t autopoll_cycle_delay_ms;
    uint8_t write_allow_regs[SMI_CFG_WRITE_REGS_MAX];
    size_t write_allow_regs_len;
    uint8_t write_deny_regs[SMI_CFG_WRITE_REGS_MAX];
    size_t write_deny_regs_len;
    smi_setting_entry_t settings[SMI_CFG_SETTINGS_MAX];
    size_t settings_len;
} smi_phy_config_t;

#endif /* BVSTK_SHARED_CONFIG_MODEL_H */
