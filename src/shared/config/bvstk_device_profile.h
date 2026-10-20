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
/* Sidecar descriptions are not device profiles, even when stored beside them. */
int bvstk_device_profile_is_register_map(const char *filename);
/* Canonical [<switch>/...]<device>/<device>.json, legacy profile.json,
 * or a flat legacy leaf.json. */
int bvstk_device_profile_path_valid(const char *path);
int bvstk_device_profile_same_file(const char *a, const char *b);
/* Preserve the existing bundle hierarchy and canonicalize its JSON leaf. */
int bvstk_device_profile_save_path(const char *id, const char *existing,
                                  char *out, size_t capacity);
/* Directory nesting must match the declared immediate parent. Flat legacy
 * profiles have no parent. The loader checks the complete graph separately. */
int bvstk_device_profile_location_valid(const char *path, const char *id,
                                       const char *parent);

#endif
