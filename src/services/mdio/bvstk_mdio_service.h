#ifndef BVSTK_MDIO_SERVICE_H
#define BVSTK_MDIO_SERVICE_H
#include "drivers/pl/mdio/bvstk_mdio_core.h"
#include "shared/config/bvstk_mdio_config_codec.h"
#include "shared/events/bvstk_event.h"
#include "shared/interfaces/bvstk_cache_sink.h"

typedef struct {
    void *context;
    bvstk_status_t (*read)(void *, uint8_t, uint8_t, uint16_t *, uint32_t);
    bvstk_status_t (*write)(void *, uint8_t, uint8_t, uint16_t, bvstk_event_source_t, uint32_t);
    bvstk_status_t (*take_write)(void *, uint8_t, uint8_t, uint16_t *);
    bvstk_status_t (*publish)(void *, uint8_t, uint8_t, uint16_t);
} bvstk_mdio_bus_ops_t;

typedef struct {
    char name[MDIO_CFG_NAME_MAX];
    uint8_t phy_addr, reg_count;
    mdio_policy_t policy;
} bvstk_mdio_device_info_t;

typedef struct {
    uint16_t value;
    uint8_t valid, seeded;
    uint64_t updated_ms;
} bvstk_mdio_cache_entry_t;

typedef struct {
    uint32_t reads, writes, denied, errors, observed, unknown_phy;
    uint8_t last_phy, last_reg, last_source;
    uint16_t last_value;
    bvstk_status_t last_status;
} bvstk_mdio_stats_t;

typedef struct {
    bvstk_mdio_core_t *core;
    bvstk_mdio_bus_ops_t bus;
    bvstk_event_sink_t events;
    bvstk_cache_sink_t cache_sink;
    void (*cache_config_changed)(void *, size_t, const mdio_phy_config_t *);
    /* Assign an OS mutex before sharing the initialized service with tasks. */
    bvstk_mutex_t mutex;
    mdio_phy_config_t devices[MDIO_CFG_MAX_DEVICES];
    bvstk_mdio_cache_entry_t cache[MDIO_CFG_MAX_DEVICES][32];
    bvstk_mdio_stats_t stats;
    uint64_t poll_due[MDIO_CFG_MAX_DEVICES], probe_due[MDIO_CFG_MAX_DEVICES];
    uint8_t poll_index[MDIO_CFG_MAX_DEVICES], online[MDIO_CFG_MAX_DEVICES];
    size_t device_count, scan_index, poll_device;
    uint8_t initialized;
} bvstk_mdio_service_t;

bvstk_status_t bvstk_mdio_service_init(bvstk_mdio_service_t *, bvstk_mdio_core_t *,
    const bvstk_mdio_bus_ops_t *, const mdio_phy_config_t *, size_t, const bvstk_event_sink_t *);
void bvstk_mdio_service_shutdown(bvstk_mdio_service_t *);
size_t bvstk_mdio_service_device_count(const bvstk_mdio_service_t *);
bvstk_status_t bvstk_mdio_service_device_info(const bvstk_mdio_service_t *, size_t, bvstk_mdio_device_info_t *);
bvstk_status_t bvstk_mdio_service_find_by_name(const bvstk_mdio_service_t *, const char *, size_t *);
bvstk_status_t bvstk_mdio_service_find_by_phy(const bvstk_mdio_service_t *, uint8_t, size_t *);
bvstk_status_t bvstk_mdio_service_read(bvstk_mdio_service_t *, size_t, uint8_t, uint16_t *, uint32_t);
bvstk_status_t bvstk_mdio_service_read_from(bvstk_mdio_service_t *, size_t, uint8_t,
    uint16_t *, bvstk_event_source_t, uint32_t);
bvstk_status_t bvstk_mdio_service_read_cached(const bvstk_mdio_service_t *, size_t, uint8_t, uint16_t *);
bvstk_status_t bvstk_mdio_service_cache_entry(const bvstk_mdio_service_t *, size_t, uint8_t, bvstk_mdio_cache_entry_t *);
bvstk_status_t bvstk_mdio_service_invalidate(bvstk_mdio_service_t *, size_t);
bvstk_status_t bvstk_mdio_service_write(bvstk_mdio_service_t *, size_t, uint8_t, uint16_t, bvstk_event_source_t, uint32_t);
bvstk_status_t bvstk_mdio_service_set_policy(bvstk_mdio_service_t *, size_t, mdio_policy_t);
bvstk_status_t bvstk_mdio_service_get_config(const bvstk_mdio_service_t *, size_t, mdio_phy_config_t *);
bvstk_status_t bvstk_mdio_service_set_config(bvstk_mdio_service_t *, size_t, const mdio_phy_config_t *);
bvstk_status_t bvstk_mdio_service_stats(const bvstk_mdio_service_t *, bvstk_mdio_stats_t *);
/* Bounded cooperative worker: up to four captured writes and one polling item.
 * Requests can coalesce in PL; observed is NOT a count of all on-wire frames. */
bvstk_status_t bvstk_mdio_service_step(bvstk_mdio_service_t *, uint64_t now_ms, uint32_t timeout_ms);
bvstk_status_t bvstk_mdio_service_poll(bvstk_mdio_service_t *, uint32_t timeout_ms);
#endif
