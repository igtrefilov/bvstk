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

int start_config_store(void);
int config_store_is_ready(void);
int config_store_wait_ready(uint32_t timeout_ms);
int config_store_get_network(network_config_t *out);

#endif /* CONFIG_STORE_H */

