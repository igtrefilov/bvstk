#ifndef BVSTK_MDIO_CONFIG_CODEC_H
#define BVSTK_MDIO_CONFIG_CODEC_H
#include "shared/config/bvstk_config_model.h"
#include "shared/base/bvstk_status.h"

bvstk_status_t bvstk_mdio_config_validate(const mdio_phy_config_t *config);
/* Strict, bounded JSON. Legacy register-only lists are converted to mask=0. */
bvstk_status_t bvstk_mdio_config_parse(const char *json, mdio_phy_config_t *out);
bvstk_status_t bvstk_mdio_config_format(const mdio_phy_config_t *config,
    char *out, size_t capacity, size_t *length);
int bvstk_mdio_policy_allows(const mdio_phy_config_t *config, uint8_t reg, uint16_t value);
#endif
