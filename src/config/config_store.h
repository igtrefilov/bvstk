#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

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
#define I2C_CFG_RULES_MAX 256u
#define I2C_CFG_AUTOPOLL_REGS_MAX 64u
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

    bool autopoll_enabled;
    uint8_t autopoll_regs[I2C_CFG_AUTOPOLL_REGS_MAX];
    size_t autopoll_regs_len;
    uint32_t autopoll_reg_delay_ms;
    uint32_t autopoll_cycle_delay_ms;

    i2c_rule_entry_t whitelist[I2C_CFG_RULES_MAX];
    size_t whitelist_len;

    i2c_rule_entry_t blacklist[I2C_CFG_RULES_MAX];
    size_t blacklist_len;

    /*
     * Persisted device settings (register writes).
     * Semantics: list of {reg,val} pairs to apply on boot.
     * This is intentionally separate from policy rules.
     */
    i2c_rule_entry_t settings[I2C_CFG_SETTINGS_MAX];
    size_t settings_len;
} i2c_device_config_t;

/*
 * SMI/MDIO PHY configuration (e.g. LAN8720).
 *
 * Used to restrict write access (safety) and to define periodic polling.
 * Files live on target as: flash:/config/smi/*.json (legacy: flash:/configs/smi/*.json)
 */
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

    /* Persisted settings (register writes), applied on boot. */
    smi_setting_entry_t settings[SMI_CFG_SETTINGS_MAX];
    size_t settings_len;
} smi_phy_config_t;

int start_config_store(void);
int config_store_is_ready(void);
int config_store_wait_ready(uint32_t timeout_ms);
int config_store_get_network(network_config_t *out);
int config_store_set_network(const network_config_t *cfg);
int config_store_save_network(void);

size_t config_store_get_i2c_device_count(void);
const i2c_device_config_t *config_store_get_i2c_devices(void);
const i2c_device_config_t *config_store_find_i2c_device_by_name(const char *name);
const i2c_device_config_t *config_store_find_i2c_device_by_addr(uint8_t addr_7b);
int config_store_set_i2c_device(const i2c_device_config_t *cfg);
int config_store_save_i2c_device(const i2c_device_config_t *cfg);

size_t config_store_get_smi_device_count(void);
const smi_phy_config_t *config_store_get_smi_devices(void);
const smi_phy_config_t *config_store_find_smi_device_by_name(const char *name);
const smi_phy_config_t *config_store_find_smi_device_by_phy(uint8_t phy_addr);
int config_store_set_smi_device(const smi_phy_config_t *cfg);
int config_store_save_smi_device(const smi_phy_config_t *cfg);

#endif /* CONFIG_STORE_H */
