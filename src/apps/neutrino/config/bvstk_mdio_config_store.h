#ifndef BVSTK_NEUTRINO_MDIO_CONFIG_STORE_H
#define BVSTK_NEUTRINO_MDIO_CONFIG_STORE_H
#include "shared/config/bvstk_config_model.h"

typedef struct {
    mdio_phy_config_t devices[MDIO_CFG_MAX_DEVICES];
    size_t device_count;
    char primary_dir[128];
    uint8_t initialized;
} bvstk_neutrino_mdio_config_store_t;

int bvstk_neutrino_mdio_config_store_init(bvstk_neutrino_mdio_config_store_t *,
    const char *primary_dir, const char *legacy_dir);
const mdio_phy_config_t *bvstk_neutrino_mdio_config_store_get(
    const bvstk_neutrino_mdio_config_store_t *, size_t);
int bvstk_neutrino_mdio_config_store_update(bvstk_neutrino_mdio_config_store_t *,
    size_t, const mdio_phy_config_t *);
int bvstk_neutrino_mdio_config_store_save(const bvstk_neutrino_mdio_config_store_t *,
    const mdio_phy_config_t *);
#endif
