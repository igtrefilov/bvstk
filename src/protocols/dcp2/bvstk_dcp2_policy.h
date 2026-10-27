#ifndef BVSTK_DCP2_POLICY_H
#define BVSTK_DCP2_POLICY_H

#include "protocols/dcp2/bvstk_dcp2_control.h"

#define BVSTK_DCP2_POLICY_VERSION 1U
#define BVSTK_DCP2_POLICY_NAME_MAX 32U
#define BVSTK_DCP2_POLICY_PAGE_HEADER 16U
#define BVSTK_DCP2_POLICY_SNAPSHOT_MS 30000U
#define BVSTK_DCP2_POLICY_STALE_SNAPSHOT 0x0301U

typedef struct {
    uint16_t count, ids_size;
    uint16_t offset[I2C_CFG_MAX_REG_COUNT];
    char ids[BVSTK_PROFILE_IDS_CAP];
} bvstk_dcp2_policy_catalog_t;

typedef struct {
    bvstk_i2c_master_service_t *i2c;
    bvstk_mdio_service_t *mdio;
    void *context;
    /* Caller serializes configuration changes; the callback copies metadata. */
    bvstk_status_t (*catalog)(void *, uint8_t service, size_t device_id,
                             bvstk_dcp2_policy_catalog_t *);
    /* Mirror an applied policy to RAM configuration, without saving or I/O. */
    bvstk_status_t (*changed)(void *, uint8_t service, size_t device_id);
    bvstk_dcp2_policy_save_fn save;
} bvstk_dcp2_policy_adapter_t;

/* One frozen snapshot per TCP connection. Keep this outside small task stacks. */
typedef struct {
    union {
        bvstk_i2c_policy_entry_t i2c;
        bvstk_mdio_policy_entry_t mdio;
    } policy;
    bvstk_dcp2_policy_catalog_t catalog;
    char subsystem[BVSTK_DCP2_POLICY_NAME_MAX], device[BVSTK_DCP2_POLICY_NAME_MAX];
    uint64_t activity_ms;
    uint32_t snapshot_id, next_snapshot;
    uint8_t service;
} bvstk_dcp2_policy_session_t;

void bvstk_dcp2_policy_init(bvstk_dcp2_policy_session_t *);
bvstk_status_t bvstk_dcp2_policy_catalog_from_profile(
    bvstk_dcp2_policy_catalog_t *, const bvstk_profile_metadata_t *, size_t count);
/* Returns a wire status, as with CACHE. Output excludes the common status. */
uint16_t bvstk_dcp2_policy_request(bvstk_dcp2_policy_session_t *,
    const bvstk_dcp2_policy_adapter_t *, uint8_t opcode,
    const uint8_t *body, size_t size, uint64_t now_ms,
    uint8_t *out, size_t capacity, uint16_t *out_size);

#endif
