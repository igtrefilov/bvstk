#include "services/cache/bvstk_cache_bridges.h"
#include "shared/interfaces/bvstk_clock.h"
#include <stdio.h>
#include <string.h>

static void resource_name(bvstk_cache_entry_t *entry, const bvstk_profile_metadata_t *p, unsigned r)
{
    if (p->version == 2U && p->id_offset[r] < sizeof(p->ids))
        (void)snprintf(entry->id, sizeof(entry->id), "%s", p->ids + p->id_offset[r]);
    else (void)snprintf(entry->id, sizeof(entry->id), "reg_%02x", r);
}
static unsigned origin(int seeded) { return seeded ? BVSTK_CACHE_SOURCE_SETTINGS : BVSTK_CACHE_SOURCE_DEVICE; }

typedef struct {
    bvstk_cache_i2c_bridge_t *bridge;
    const i2c_device_config_t *config;
    size_t device;
} i2c_description_t;
static void describe_i2c(void *context, unsigned r, bvstk_cache_entry_t *entry)
{
    const i2c_description_t *c = context;
    const bvstk_i2c_cache_t *cache = c->bridge->service->cache;
    resource_name(entry, &c->config->profile, r);
    entry->width_bits = 8; entry->value = cache->values[c->device][r];
    entry->valid = cache->valid[c->device][r];
    entry->source = (uint8_t)origin(cache->seeded[c->device][r]);
    entry->updated_ms = cache->updated_ms[c->device][r];
}
static void configure_i2c(void *context, size_t d, const i2c_device_config_t *config)
{
    bvstk_cache_i2c_bridge_t *b = context;
    i2c_description_t description = { b, config, d };
    b->status = bvstk_cache_define(b->registry, &b->handles[d], "i2c", config->name,
        config->reg_count, b->service->presence[d], describe_i2c, &description);
}
static void i2c_enter(void *context)
{
    bvstk_cache_registry_t *r = ((bvstk_cache_i2c_bridge_t *)context)->registry;
    if (r->guard.enter) r->guard.enter(r->guard.context);
}
static void i2c_leave(void *context)
{
    bvstk_cache_registry_t *r = ((bvstk_cache_i2c_bridge_t *)context)->registry;
    if (r->guard.leave) r->guard.leave(r->guard.context);
}
static void i2c_entry(void *context, size_t d, unsigned r, uint64_t v, int valid, int seeded, uint64_t time)
{
    bvstk_cache_i2c_bridge_t *b = context;
    /* A shrinking catalog can invalidate slots outside its exported range. */
    if (d < b->service->devices->count && r < b->service->devices->items[d].reg_count)
        (void)bvstk_cache_put(b->registry, b->handles[d], r, v, valid, origin(seeded), time, bvstk_platform_now_ms());
}
static void i2c_presence(void *context, size_t d, unsigned presence)
{
    bvstk_cache_i2c_bridge_t *b = context;
    (void)bvstk_cache_presence(b->registry, b->handles[d], presence, bvstk_platform_now_ms());
}
bvstk_status_t bvstk_cache_attach_i2c(bvstk_cache_i2c_bridge_t *b, bvstk_cache_registry_t *registry,
    bvstk_i2c_master_service_t *service, const i2c_device_config_t *configs)
{
    size_t d;
    memset(b, 0, sizeof(*b)); b->registry = registry; b->service = service;
    for (d = 0; d < service->cache->device_count; ++d) {
        configure_i2c(b, d, &configs[d]);
        if (b->status != BVSTK_OK) return b->status;
    }
    service->cache->sink.context = b;
    service->cache->sink.enter = i2c_enter; service->cache->sink.leave = i2c_leave;
    service->cache->sink.entry = i2c_entry; service->cache->sink.presence = i2c_presence;
    service->cache_config_changed = configure_i2c;
    return BVSTK_OK;
}

typedef struct {
    bvstk_cache_mdio_bridge_t *bridge;
    const mdio_phy_config_t *config;
    size_t device;
} mdio_description_t;
static void describe_mdio(void *context, unsigned r, bvstk_cache_entry_t *entry)
{
    const mdio_description_t *c = context;
    const bvstk_mdio_cache_entry_t *e = &c->bridge->service->cache[c->device][r];
    resource_name(entry, &c->config->profile, r);
    entry->width_bits = 16; entry->value = e->value; entry->valid = e->valid;
    entry->source = (uint8_t)origin(e->seeded); entry->updated_ms = e->updated_ms;
}
static void configure_mdio(void *context, size_t d, const mdio_phy_config_t *config)
{
    bvstk_cache_mdio_bridge_t *b = context;
    mdio_description_t description = { b, config, d };
    b->status = bvstk_cache_define(b->registry, &b->handles[d], "mdio", config->name,
        config->reg_count, b->service->online[d] ? BVSTK_CACHE_PRESENCE_READY : BVSTK_CACHE_PRESENCE_UNKNOWN,
        describe_mdio, &description);
}
static void mdio_entry(void *context, size_t d, unsigned r, uint64_t v, int valid, int seeded, uint64_t time)
{
    bvstk_cache_mdio_bridge_t *b = context;
    if (r < b->service->devices[d].reg_count)
        (void)bvstk_cache_put(b->registry, b->handles[d], r, v, valid, origin(seeded), time, bvstk_platform_now_ms());
}
static void mdio_presence(void *context, size_t d, unsigned presence)
{
    bvstk_cache_mdio_bridge_t *b = context;
    (void)bvstk_cache_presence(b->registry, b->handles[d], presence, bvstk_platform_now_ms());
}
bvstk_status_t bvstk_cache_attach_mdio(bvstk_cache_mdio_bridge_t *b, bvstk_cache_registry_t *registry,
    bvstk_mdio_service_t *service)
{
    size_t d;
    memset(b, 0, sizeof(*b)); b->registry = registry; b->service = service;
    for (d = 0; d < service->device_count; ++d) {
        configure_mdio(b, d, &service->devices[d]);
        if (b->status != BVSTK_OK) return b->status;
    }
    service->cache_sink.context = b; service->cache_sink.entry = mdio_entry;
    service->cache_sink.presence = mdio_presence; service->cache_config_changed = configure_mdio;
    return BVSTK_OK;
}
