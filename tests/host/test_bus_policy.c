#include "services/i2c/bvstk_i2c_master_service.h"
#include "services/mdio/bvstk_mdio_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned depth, locks, reads, writes, publishes, notifications;
    uint16_t value;
    bvstk_status_t lock_status;
} backend_t;

static bvstk_i2c_devices_t devices, devices_before;
static bvstk_i2c_cache_t cache, cache_before;
static bvstk_i2c_policy_t policy;
static bvstk_i2c_master_service_t i2c, i2c_before;
static bvstk_mdio_service_t mdio, mdio_before, mdio_compare;

uint64_t bvstk_platform_now_ms(void) { return 1000; }
void bvstk_platform_sleep_ms(uint32_t ms) { (void)ms; }
int bvstk_mmio_region_open(bvstk_mmio_region_t *r, uintptr_t p, size_t n)
{ (void)r; (void)p; (void)n; return -1; }
void bvstk_mmio_region_close(bvstk_mmio_region_t *r) { (void)r; }
int bvstk_mmio_read32(const bvstk_mmio_region_t *r, size_t n, uint32_t *v)
{ (void)r; (void)n; (void)v; return -1; }
int bvstk_mmio_write32(const bvstk_mmio_region_t *r, size_t n, uint32_t v)
{ (void)r; (void)n; (void)v; return -1; }

static void enter(void *ctx)
{ backend_t *b = ctx; ++b->depth; ++b->locks; }
static void leave(void *ctx)
{ backend_t *b = ctx; assert(b->depth); --b->depth; }
static bvstk_status_t lock_service(void *ctx, uint32_t timeout)
{
    backend_t *b = ctx;
    assert(timeout && !b->depth);
    if (b->lock_status != BVSTK_OK) return b->lock_status;
    enter(ctx); return BVSTK_OK;
}
static bvstk_status_t i2c_read(void *ctx, uint8_t addr, uint8_t reg,
    uint8_t *v, uint32_t timeout)
{
    backend_t *b = ctx;
    (void)addr; (void)reg; (void)timeout;
    assert(!b->depth); ++b->reads; *v = (uint8_t)b->value; return BVSTK_OK;
}
static bvstk_status_t i2c_write(void *ctx, uint8_t addr, uint8_t reg,
    uint8_t v, uint32_t timeout)
{
    backend_t *b = ctx;
    (void)addr; (void)reg; (void)timeout;
    assert(!b->depth); ++b->writes; b->value = v; return BVSTK_OK;
}
static bvstk_status_t mdio_read(void *ctx, uint8_t phy, uint8_t reg,
    uint16_t *v, uint32_t timeout)
{
    backend_t *b = ctx;
    (void)phy; (void)reg; (void)timeout;
    assert(b->depth == 1); ++b->reads; *v = b->value; return BVSTK_OK;
}
static bvstk_status_t mdio_write(void *ctx, uint8_t phy, uint8_t reg,
    uint16_t v, bvstk_event_source_t source, uint32_t timeout)
{
    backend_t *b = ctx;
    (void)phy; (void)reg; (void)source; (void)timeout;
    assert(b->depth == 1); ++b->writes; b->value = v; return BVSTK_OK;
}
static bvstk_status_t publish(void *ctx, uint8_t phy, uint8_t reg, uint16_t v)
{ backend_t *b = ctx; (void)phy; (void)reg; (void)v; ++b->publishes; return BVSTK_OK; }
static void i2c_changed(void *ctx, size_t d, const i2c_device_config_t *c)
{ backend_t *b = ctx; (void)d; (void)c; ++b->notifications; }
static void mdio_changed(void *ctx, size_t d, const mdio_phy_config_t *c)
{ backend_t *b = ctx; (void)d; (void)c; ++b->notifications; }

static void i2c_tests(void)
{
    backend_t b = {0};
    i2c_device_config_t c = {0};
    bvstk_i2c_policy_entry_t p, before;
    bvstk_i2c_master_io_t io = {&b, i2c_read, i2c_write};
    strcpy(c.name, "i2c"); c.addr_7b = 32; c.reg_count = 4; c.max_value_code = 255;
    c.policy = I2C_POLICY_WHITELIST;
    assert(bvstk_i2c_devices_init_from_config(&devices, &c, 1) == BVSTK_OK);
    assert(bvstk_i2c_cache_init(&cache, 1) == BVSTK_OK);
    assert(bvstk_i2c_policy_init(&policy, &c, 1) == BVSTK_OK);
    assert(bvstk_i2c_master_service_init(&i2c, NULL, &io, &devices, &cache, &policy, NULL) == BVSTK_OK);
    cache.sink = (bvstk_cache_sink_t){.context=&b, .enter=enter, .leave=leave};
    i2c.cache_config_changed = i2c_changed;
    assert(bvstk_i2c_cache_seed(&cache, 0, 3, 57) == BVSTK_OK);
    i2c.presence[0] = BVSTK_I2C_PRESENCE_RESPONDING;
    i2c_before = i2c; devices_before = devices; cache_before = cache;

    assert(bvstk_i2c_master_service_rule_allow(&i2c, 0, 1, 7) == BVSTK_OK);
    assert(bvstk_i2c_master_service_rule_deny(&i2c, 0, 1, 7) == BVSTK_OK);
    assert(bvstk_i2c_master_service_rule_allow(&i2c, 0, 1, 7) == BVSTK_OK);
    assert(bvstk_i2c_master_service_get_policy(&i2c, 0, &p) == BVSTK_OK);
    assert(p.mode == I2C_POLICY_WHITELIST && p.whitelist_len == 1 && p.blacklist_len == 1);
    assert(bvstk_i2c_master_service_rule_remove(&i2c, 0, I2C_POLICY_BLACKLIST, 1, 7) == BVSTK_OK);
    assert(bvstk_i2c_master_service_rule_remove(&i2c, 0, I2C_POLICY_BLACKLIST, 1, 7) == BVSTK_ERR_NOT_FOUND);
    assert(bvstk_i2c_master_service_rule_deny(&i2c, 0, 1, 7) == BVSTK_OK);
    assert(bvstk_i2c_master_service_rule_clear_list(&i2c, 0, I2C_POLICY_BLACKLIST) == BVSTK_OK);
    assert(bvstk_i2c_master_service_get_policy(&i2c, 0, &p) == BVSTK_OK);
    assert(p.mode == I2C_POLICY_WHITELIST && p.whitelist_len == 1 && !p.blacklist_len);
    before = p;
    assert(bvstk_i2c_master_service_set_policy(&i2c, 0, (i2c_policy_t)2) == BVSTK_ERR_MALFORMED);
    assert(bvstk_i2c_master_service_rule_clear_list(&i2c, 0, (i2c_policy_t)2) == BVSTK_ERR_MALFORMED);
    assert(bvstk_i2c_master_service_rule_remove(&i2c, 0, (i2c_policy_t)2, 1, 7) == BVSTK_ERR_MALFORMED);
    assert(bvstk_i2c_master_service_rule_remove(&i2c, 0, I2C_POLICY_WHITELIST, 4, 7) == BVSTK_ERR_RANGE);
    assert(bvstk_i2c_master_service_rule_allow(&i2c, 0, 4, 7) == BVSTK_ERR_RANGE);
    assert(bvstk_i2c_master_service_get_policy(&i2c, 1, &p) == BVSTK_ERR_NOT_FOUND);
    assert(bvstk_i2c_master_service_get_policy(&i2c, 0, NULL) == BVSTK_ERR_MALFORMED);
    assert(bvstk_i2c_master_service_get_policy(NULL, 0, &p) == BVSTK_ERR_NOT_READY);
    assert(bvstk_i2c_master_service_get_policy(&i2c, 0, &p) == BVSTK_OK && !memcmp(&before, &p, sizeof(p)));

    assert(bvstk_i2c_master_service_rule_clear_list(&i2c, 0, I2C_POLICY_WHITELIST) == BVSTK_OK);
    for (size_t n = 0; n < I2C_CFG_RULES_MAX; ++n)
        assert(bvstk_i2c_master_service_rule_allow(&i2c, 0, 0, (uint8_t)n) == BVSTK_OK);
    assert(bvstk_i2c_master_service_rule_allow(&i2c, 0, 0, 255) == BVSTK_OK);
    assert(bvstk_i2c_master_service_get_policy(&i2c, 0, &before) == BVSTK_OK);
    assert(bvstk_i2c_master_service_rule_allow(&i2c, 0, 1, 0) == BVSTK_ERR_RANGE);
    assert(bvstk_i2c_master_service_get_policy(&i2c, 0, &p) == BVSTK_OK && !memcmp(&before, &p, sizeof(p)));
    for (size_t n = 0; n < I2C_CFG_RULES_MAX; ++n)
        assert(bvstk_i2c_master_service_rule_deny(&i2c, 0, 0, (uint8_t)n) == BVSTK_OK);
    assert(bvstk_i2c_master_service_rule_deny(&i2c, 0, 0, 255) == BVSTK_OK);
    assert(bvstk_i2c_master_service_rule_deny(&i2c, 0, 1, 0) == BVSTK_ERR_RANGE);
    devices.items[0].max_value_code = 15;
    assert(bvstk_i2c_master_service_rule_allow(&i2c, 0, 1, 16) == BVSTK_ERR_RANGE);
    assert(bvstk_i2c_master_service_rule_remove(&i2c, 0, I2C_POLICY_WHITELIST, 1, 16) == BVSTK_ERR_RANGE);
    devices.items[0].max_value_code = 255;
    assert(bvstk_i2c_master_service_set_policy(&i2c, 0, I2C_POLICY_BLACKLIST) == BVSTK_OK);
    assert(!memcmp(&i2c_before, &i2c, sizeof(i2c)) && !memcmp(&devices_before, &devices, sizeof(devices)));
    assert(!memcmp(&cache_before, &cache, sizeof(cache)) && !b.notifications && !b.reads && !b.writes);
    assert(bvstk_i2c_master_service_write(&i2c, 0, 0, 7, BVSTK_EVENT_SOURCE_DCP, 5) == BVSTK_ERR_DENIED);
    assert(!b.writes);
    assert(bvstk_i2c_master_service_rule_clear_list(&i2c, 0, I2C_POLICY_BLACKLIST) == BVSTK_OK);
    assert(bvstk_i2c_master_service_write(&i2c, 0, 0, 7, BVSTK_EVENT_SOURCE_DCP, 5) == BVSTK_OK && b.writes == 1);
    assert(bvstk_i2c_master_service_set_policy(&i2c, 0, I2C_POLICY_WHITELIST) == BVSTK_OK);
    assert(bvstk_i2c_master_service_write(&i2c, 0, 1, 7, BVSTK_EVENT_SOURCE_DCP, 5) == BVSTK_ERR_DENIED);
    assert(bvstk_i2c_master_service_write(&i2c, 0, 0, 7, BVSTK_EVENT_SOURCE_DCP, 5) == BVSTK_OK && b.writes == 2);
    assert(!b.depth && b.locks > 0);
}

static void mdio_tests(void)
{
    backend_t b = {0};
    mdio_phy_config_t c[2];
    bvstk_mdio_policy_entry_t p, before;
    bvstk_mdio_bus_ops_t bus = {.context=&b, .read=mdio_read, .write=mdio_write, .publish=publish};
    memset(c, 0, sizeof(c));
    strcpy(c[0].name, "phy"); strcpy(c[0].file_name, "phy/phy.json");
    c[0].phy_addr = 1; c[0].reg_count = 32; c[0].policy = MDIO_POLICY_WHITELIST;
    c[0].settings_len = 1; c[0].settings[0] = (mdio_setting_entry_t){4, 99};
    c[0].autopoll_enabled = true; c[0].autopoll_regs_len = 1; c[0].autopoll_regs[0] = 4;
    c[0].autopoll_reg_delay_ms = 5; c[0].autopoll_cycle_delay_ms = 100;
    c[0].whitelist_len = 2;
    c[0].whitelist[0] = (mdio_rule_entry_t){2, 0xABCD, 0x00FF};
    c[0].whitelist[1] = (mdio_rule_entry_t){2, 0x12CD, 0x00FF};
    c[1] = c[0]; strcpy(c[1].name, "other"); strcpy(c[1].file_name, "other/other.json"); c[1].phy_addr = 2;
    assert(bvstk_mdio_service_init(&mdio, NULL, &bus, c, 2, NULL) == BVSTK_OK);
    mdio.mutex = (bvstk_mutex_t){&b, lock_service, leave};
    mdio.cache_sink.context = &b; mdio.cache_config_changed = mdio_changed;
    mdio.poll_due[0] = 12345; mdio.probe_due[0] = 67890; mdio.poll_index[0] = 3; mdio.online[0] = 1;
    mdio_before = mdio;
    assert(bvstk_mdio_service_get_policy(&mdio, 0, &p) == BVSTK_OK && p.whitelist[0].val == 0xABCD);
    assert(bvstk_mdio_service_rule_add(&mdio, 0, MDIO_POLICY_WHITELIST, 2, 0xFFCD, 0x00FF) == BVSTK_OK);
    assert(bvstk_mdio_service_get_policy(&mdio, 0, &p) == BVSTK_OK && p.whitelist_len == 2 && p.whitelist[0].val == 0xABCD);
    assert(bvstk_mdio_service_rule_remove(&mdio, 0, MDIO_POLICY_WHITELIST, 2, 0x99CD, 0x00FF) == BVSTK_OK);
    assert(bvstk_mdio_service_rule_remove(&mdio, 0, MDIO_POLICY_WHITELIST, 2, 0x99CD, 0x00FF) == BVSTK_ERR_NOT_FOUND);
    assert(bvstk_mdio_service_rule_add(&mdio, 0, MDIO_POLICY_WHITELIST, 4, 0xABCD, 0x00FF) == BVSTK_OK);
    assert(bvstk_mdio_service_rule_add(&mdio, 0, MDIO_POLICY_BLACKLIST, 4, 0xABCD, 0x00FF) == BVSTK_OK);
    assert(bvstk_mdio_service_rule_add(&mdio, 0, MDIO_POLICY_WHITELIST, 4, 0x12CD, 0x00FF) == BVSTK_OK);
    assert(bvstk_mdio_service_get_policy(&mdio, 0, &p) == BVSTK_OK && p.whitelist_len == 1 && p.whitelist[0].val == 0xCD);
    assert(bvstk_mdio_service_rule_remove(&mdio, 0, MDIO_POLICY_BLACKLIST, 4, 0xFFCD, 0x00FF) == BVSTK_OK);
    assert(bvstk_mdio_service_get_policy(&mdio, 0, &before) == BVSTK_OK && before.whitelist_len == 1 && !before.blacklist_len);
    assert(bvstk_mdio_service_set_policy(&mdio, 0, (mdio_policy_t)2) == BVSTK_ERR_MALFORMED);
    assert(bvstk_mdio_service_rule_add(&mdio, 0, (mdio_policy_t)2, 0, 0, 0) == BVSTK_ERR_MALFORMED);
    assert(bvstk_mdio_service_rule_remove(&mdio, 0, (mdio_policy_t)2, 0, 0, 0) == BVSTK_ERR_MALFORMED);
    assert(bvstk_mdio_service_rule_clear_list(&mdio, 0, (mdio_policy_t)2) == BVSTK_ERR_MALFORMED);
    assert(bvstk_mdio_service_rule_add(&mdio, 0, MDIO_POLICY_WHITELIST, 32, 0, 0) == BVSTK_ERR_RANGE);
    assert(bvstk_mdio_service_rule_remove(&mdio, 0, MDIO_POLICY_WHITELIST, 32, 0, 0) == BVSTK_ERR_RANGE);
    assert(bvstk_mdio_service_get_policy(&mdio, 2, &p) == BVSTK_ERR_NOT_FOUND);
    assert(bvstk_mdio_service_get_policy(&mdio, 0, NULL) == BVSTK_ERR_MALFORMED);
    assert(bvstk_mdio_service_get_policy(NULL, 0, &p) == BVSTK_ERR_NOT_READY);
    assert(bvstk_mdio_service_get_policy(&mdio, 0, &p) == BVSTK_OK && !memcmp(&before, &p, sizeof(p)));
    b.lock_status = BVSTK_ERR_BUSY;
    assert(bvstk_mdio_service_rule_add(&mdio, 0, MDIO_POLICY_WHITELIST, 0, 0, 0) == BVSTK_ERR_BUSY);
    assert(bvstk_mdio_service_get_policy(&mdio, 0, &p) == BVSTK_ERR_BUSY);
    b.lock_status = BVSTK_OK;
    assert(bvstk_mdio_service_rule_clear_list(&mdio, 0, MDIO_POLICY_WHITELIST) == BVSTK_OK);
    for (unsigned list = 0; list < 2; ++list) {
        for (size_t n = 0; n < MDIO_CFG_RULES_MAX; ++n)
            assert(bvstk_mdio_service_rule_add(&mdio, 0, (mdio_policy_t)list, 4, (uint16_t)n, 0xFFFF) == BVSTK_OK);
        assert(bvstk_mdio_service_rule_add(&mdio, 0, (mdio_policy_t)list, 4, 63, 0xFFFF) == BVSTK_OK);
        assert(bvstk_mdio_service_get_policy(&mdio, 0, &before) == BVSTK_OK);
        assert(bvstk_mdio_service_rule_add(&mdio, 0, (mdio_policy_t)list, 4, 64, 0xFFFF) == BVSTK_ERR_RANGE);
        assert(bvstk_mdio_service_get_policy(&mdio, 0, &p) == BVSTK_OK && !memcmp(&before, &p, sizeof(p)));
    }
    assert(bvstk_mdio_service_set_policy(&mdio, 0, MDIO_POLICY_BLACKLIST) == BVSTK_OK);
    assert(bvstk_mdio_service_rule_clear_list(&mdio, 0, MDIO_POLICY_BLACKLIST) == BVSTK_OK);
    assert(bvstk_mdio_service_get_policy(&mdio, 0, &p) == BVSTK_OK && p.whitelist_len == 64 && !p.blacklist_len && p.mode == MDIO_POLICY_BLACKLIST);
    mdio_compare = mdio;
    mdio_compare.devices[0].policy = mdio_before.devices[0].policy;
    memcpy(mdio_compare.devices[0].whitelist, mdio_before.devices[0].whitelist, sizeof(c[0].whitelist));
    memcpy(mdio_compare.devices[0].blacklist, mdio_before.devices[0].blacklist, sizeof(c[0].blacklist));
    mdio_compare.devices[0].whitelist_len = mdio_before.devices[0].whitelist_len;
    mdio_compare.devices[0].blacklist_len = mdio_before.devices[0].blacklist_len;
    assert(!memcmp(&mdio_compare, &mdio_before, sizeof(mdio)) && b.publishes == 2 && !b.notifications && !b.reads && !b.writes);

    assert(bvstk_mdio_service_rule_clear_list(&mdio, 0, MDIO_POLICY_WHITELIST) == BVSTK_OK);
    assert(bvstk_mdio_service_rule_add(&mdio, 0, MDIO_POLICY_WHITELIST, 4, 0xABCD, 0x00FF) == BVSTK_OK);
    assert(bvstk_mdio_service_set_policy(&mdio, 0, MDIO_POLICY_WHITELIST) == BVSTK_OK);
    assert(bvstk_mdio_service_write(&mdio, 0, 4, 0x12CE, BVSTK_EVENT_SOURCE_DCP, 5) == BVSTK_ERR_DENIED && !b.writes);
    assert(bvstk_mdio_service_write(&mdio, 0, 4, 0x12CD, BVSTK_EVENT_SOURCE_DCP, 5) == BVSTK_OK && b.writes == 1);
    assert(bvstk_mdio_service_rule_add(&mdio, 0, MDIO_POLICY_WHITELIST, 5, 0xFFFF, 0) == BVSTK_OK);
    assert(bvstk_mdio_service_write(&mdio, 0, 5, 0, BVSTK_EVENT_SOURCE_DCP, 5) == BVSTK_OK && b.writes == 2);
    assert(bvstk_mdio_service_write(&mdio, 0, 5, 0xFFFF, BVSTK_EVENT_SOURCE_DCP, 5) == BVSTK_OK && b.writes == 3);
    assert(bvstk_mdio_service_rule_add(&mdio, 0, MDIO_POLICY_BLACKLIST, 5, 0, 0) == BVSTK_OK);
    assert(bvstk_mdio_service_set_policy(&mdio, 0, MDIO_POLICY_BLACKLIST) == BVSTK_OK);
    assert(bvstk_mdio_service_write(&mdio, 0, 5, 0x8000, BVSTK_EVENT_SOURCE_DCP, 5) == BVSTK_ERR_DENIED && b.writes == 3);
    assert(bvstk_mdio_service_write(&mdio, 0, 4, 0xFFFF, BVSTK_EVENT_SOURCE_DCP, 5) == BVSTK_OK && b.writes == 4);
    assert(!b.depth && b.locks > 0);
}

int main(void)
{
    i2c_tests(); mdio_tests();
    puts("I2C/MDIO policy snapshots, bounds, capacity, list isolation, masks and enforcement passed");
    return 0;
}
