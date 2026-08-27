#ifndef BVSTK_NEUTRINO_I2C_CONFIG_STORE_H
#define BVSTK_NEUTRINO_I2C_CONFIG_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "shared/config/bvstk_config_model.h"

#define BVSTK_NEUTRINO_I2C_CONFIG_DIR "/flash/config/i2c"
#define BVSTK_NEUTRINO_I2C_LEGACY_DIR "/flash/configs/i2c"

typedef struct {
    i2c_device_config_t devices[I2C_CFG_MAX_DEVICES];
    size_t device_count;
    char primary_dir[128];
    uint8_t initialized;
} bvstk_neutrino_i2c_config_store_t;

int bvstk_neutrino_i2c_config_store_init(
    bvstk_neutrino_i2c_config_store_t *store,
    const char *primary_dir,
    const char *legacy_dir);
void bvstk_neutrino_i2c_config_store_shutdown(
    bvstk_neutrino_i2c_config_store_t *store);

size_t bvstk_neutrino_i2c_config_store_count(
    const bvstk_neutrino_i2c_config_store_t *store);
const i2c_device_config_t *bvstk_neutrino_i2c_config_store_devices(
    const bvstk_neutrino_i2c_config_store_t *store);
const i2c_device_config_t *bvstk_neutrino_i2c_config_store_get(
    const bvstk_neutrino_i2c_config_store_t *store,
    size_t device_id);
const i2c_device_config_t *bvstk_neutrino_i2c_config_store_find_name(
    const bvstk_neutrino_i2c_config_store_t *store,
    const char *name,
    size_t *device_id);
const i2c_device_config_t *bvstk_neutrino_i2c_config_store_find_addr(
    const bvstk_neutrino_i2c_config_store_t *store,
    uint8_t addr_7b,
    size_t *device_id);

int bvstk_neutrino_i2c_config_store_update(
    bvstk_neutrino_i2c_config_store_t *store,
    size_t device_id,
    const i2c_device_config_t *config);
int bvstk_neutrino_i2c_config_store_save(
    const bvstk_neutrino_i2c_config_store_t *store,
    const i2c_device_config_t *config);

#endif /* BVSTK_NEUTRINO_I2C_CONFIG_STORE_H */
