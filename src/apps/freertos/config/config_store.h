#ifndef BVSTK_FREERTOS_CONFIG_STORE_H
#define BVSTK_FREERTOS_CONFIG_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "shared/config/bvstk_config_model.h"

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

#endif /* BVSTK_FREERTOS_CONFIG_STORE_H */
