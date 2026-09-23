#ifndef BVSTK_DEVICE_PROFILE_H
#define BVSTK_DEVICE_PROFILE_H

#include "shared/base/bvstk_status.h"
#include "shared/config/bvstk_config_model.h"

/* Strict JSON/version gate. An explicit unsupported version is never retried
 * as legacy input. Version zero denotes a syntactically valid legacy object. */
bvstk_status_t bvstk_device_profile_version(const char *json, unsigned *version);

/* Supported v2 adapters: bvstk_i2c_bridge and bvstk_mdio_bridge. No I/O, heap,
 * process-global parser state, or silently ignored behavioural options. */
bvstk_status_t bvstk_device_profile_parse_i2c(const char *, i2c_device_config_t *);
bvstk_status_t bvstk_device_profile_parse_mdio(const char *, mdio_phy_config_t *);
bvstk_status_t bvstk_device_profile_format_i2c(const i2c_device_config_t *, char *, size_t, size_t *);
bvstk_status_t bvstk_device_profile_format_mdio(const mdio_phy_config_t *, char *, size_t, size_t *);
bvstk_status_t bvstk_device_profile_metadata_validate(const bvstk_profile_metadata_t *, size_t);

#endif
