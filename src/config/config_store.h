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

typedef enum {
    AXP_POLICY_WHITELIST = 0,
    AXP_POLICY_BLACKLIST = 1
} axp_policy_t;

typedef struct {
    uint8_t reg;
    uint8_t val;
} axp_rule_entry_t;

#define AXP15060_RULES_MAX 256u
#define AXP15060_AUTOPOLL_REGS_MAX 64u

typedef struct {
    axp_policy_t policy;

    axp_rule_entry_t whitelist[AXP15060_RULES_MAX];
    size_t whitelist_len;

    axp_rule_entry_t blacklist[AXP15060_RULES_MAX];
    size_t blacklist_len;

    bool autopoll_enabled;
    uint8_t autopoll_regs[AXP15060_AUTOPOLL_REGS_MAX];
    size_t autopoll_regs_len;
    uint32_t autopoll_reg_delay_ms;
    uint32_t autopoll_cycle_delay_ms;
} axp15060_config_t;

int start_config_store(void);
int config_store_is_ready(void);
int config_store_wait_ready(uint32_t timeout_ms);
int config_store_get_network(network_config_t *out);
int config_store_set_network(const network_config_t *cfg);
int config_store_save_network(void);

int config_store_get_axp15060(axp15060_config_t *out);
const axp15060_config_t *config_store_peek_axp15060(void);
int config_store_set_axp15060(const axp15060_config_t *cfg);
int config_store_set_axp15060_defaults(void);
int config_store_save_axp15060(void);

#endif /* CONFIG_STORE_H */
