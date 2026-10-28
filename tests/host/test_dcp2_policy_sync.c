/* Real runtime synchronization and Neutrino console editing; hardware startup
 * is discarded by --gc-sections. Storage adapters record RAM and save calls. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Override only this translation unit's otherwise empty host critical macros. */
#include "task.h"
static unsigned critical_depth, critical_enters, critical_exits, store_calls;
static unsigned competing_edits;
static int schedule_competing_edit, competing_edit_pending;
static void run_competing_edit(void);
static void enter_critical(void)
{
    ++critical_depth;
    ++critical_enters;
}
static void exit_critical(void)
{
    assert(critical_depth > 0);
    --critical_depth;
    ++critical_exits;
    if (!critical_depth && competing_edit_pending) {
        competing_edit_pending = 0;
        run_competing_edit();
    }
}
#undef taskENTER_CRITICAL
#undef taskEXIT_CRITICAL
#define taskENTER_CRITICAL() enter_critical()
#define taskEXIT_CRITICAL() exit_critical()

#include "apps/freertos/runtime/bvstk_runtime.c"
#include "apps/freertos/services/dcp2/dcp2_policy_store.c"
#include "apps/neutrino/runtime/bvstk_i2c_runtime.c"
#include "apps/neutrino/bvstkd/bvstk_dcp2_runtime.h"

static i2c_device_config_t freertos_stored, saved;
static mdio_phy_config_t mdio_saved;
static unsigned saves;
static int store_ok = 1, save_ok = 1, store_present = 1, store_ready = 1;
static bvstk_neutrino_i2c_runtime_t nto;
static int allow_bus;
static unsigned bus_reads, bus_writes;
static uint8_t bus_value;
static uint8_t last_address, last_register;
static int bus_locked;

/* No hardware register access is permitted by these configuration scenarios. */
int bvstk_mmio_region_open(bvstk_mmio_region_t *r, uintptr_t base, size_t size)
{ (void)r; (void)base; (void)size; assert(!"unexpected MMIO mapping"); return -1; }
void bvstk_mmio_region_close(bvstk_mmio_region_t *r) { (void)r; }
int bvstk_mmio_read32(const bvstk_mmio_region_t *r, size_t offset, uint32_t *value)
{ (void)r; (void)offset; (void)value; assert(!"unexpected MMIO read"); return -1; }
int bvstk_mmio_write32(const bvstk_mmio_region_t *r, size_t offset, uint32_t value)
{ (void)r; (void)offset; (void)value; assert(!"unexpected MMIO write"); return -1; }
void bvstk_platform_sleep_ms(uint32_t milliseconds) { (void)milliseconds; }

int config_store_is_ready(void) { return store_ready; }
int config_store_save_i2c_device(const i2c_device_config_t *config)
{
    assert(critical_depth == 0);
    ++saves;
    if (save_ok) saved = *config;
    return save_ok;
}
int config_store_save_mdio_device(const mdio_phy_config_t *config)
{
    assert(critical_depth == 0);
    ++saves;
    if (save_ok) mdio_saved = *config;
    return save_ok;
}
uint64_t bvstk_platform_now_ms(void) { return 1000; }
const i2c_device_config_t *config_store_find_i2c_device_by_name(const char *name)
{
    assert(critical_depth == 1);
    /* Runtime has already copied the service policy when it looks up the mirror.
     * Model a second task becoming ready here, then running at section exit.
     * This is a deterministic deferral model, not a host scheduler emulation. */
    if (schedule_competing_edit) {
        schedule_competing_edit = 0;
        competing_edit_pending = 1;
        assert(competing_edits == 0);
    }
    return store_present && !strcmp(name, freertos_stored.name) ? &freertos_stored : NULL;
}
int config_store_set_i2c_device(const i2c_device_config_t *config)
{
    assert(critical_depth == 1);
    ++store_calls;
    if (competing_edit_pending) assert(competing_edits == 0);
    if (store_ok) freertos_stored = *config;
    return store_ok;
}
const i2c_device_config_t *bvstk_neutrino_i2c_config_store_get(
    const bvstk_neutrino_i2c_config_store_t *store, size_t id)
{
    return id < store->device_count ? &store->devices[id] : NULL;
}
size_t bvstk_neutrino_i2c_config_store_count(const bvstk_neutrino_i2c_config_store_t *store)
{
    return store->device_count;
}
const i2c_device_config_t *bvstk_neutrino_i2c_config_store_find_name(
    const bvstk_neutrino_i2c_config_store_t *store, const char *name, size_t *id)
{
    for (size_t i = 0; i < store->device_count; ++i)
        if (!strcasecmp(store->devices[i].name, name)) { *id = i; return &store->devices[i]; }
    return NULL;
}
const i2c_device_config_t *bvstk_neutrino_i2c_config_store_find_addr(
    const bvstk_neutrino_i2c_config_store_t *store, uint8_t address, size_t *id)
{
    const i2c_device_config_t *found = NULL;
    for (size_t i = 0; i < store->device_count; ++i)
        if (bvstk_i2c_address_space(&store->devices[i].endpoint,
                                  store->devices[i].addr_7b, address) >= 0) {
            if (found) return NULL;
            *id = i; found = &store->devices[i];
        }
    return found;
}
int bvstk_neutrino_i2c_config_store_update(bvstk_neutrino_i2c_config_store_t *store,
    size_t id, const i2c_device_config_t *config)
{
    if (!store_ok || id >= store->device_count) return 0;
    store->devices[id] = *config;
    return 1;
}
int bvstk_neutrino_i2c_config_store_save(const bvstk_neutrino_i2c_config_store_t *store,
    const i2c_device_config_t *config)
{
    (void)store;
    ++saves;
    if (save_ok) saved = *config;
    return save_ok;
}
/* A policy-only console edit must never change hardware identity. */
size_t bvstk_i2c_slave_service_target(const bvstk_i2c_slave_service_t *service)
{
    (void)service;
    assert(!"unexpected slave target query");
    return 0;
}
bvstk_status_t bvstk_i2c_slave_hw_set_address(bvstk_i2c_slave_hw_t *hw, uint8_t address)
{
    (void)hw; (void)address;
    assert(!"unexpected slave hardware change");
    return BVSTK_ERR_IO;
}
static bvstk_status_t no_read(void *ctx, uint8_t addr, uint8_t reg, uint8_t *value, uint32_t timeout)
{
    (void)ctx; (void)addr; (void)reg; (void)value; (void)timeout;
    assert(allow_bus);
    ++bus_reads; *value = bus_value;
    return BVSTK_OK;
}
static bvstk_status_t no_write(void *ctx, uint8_t addr, uint8_t reg, uint8_t value, uint32_t timeout)
{
    (void)ctx; (void)addr; (void)reg; (void)value; (void)timeout;
    assert(allow_bus);
    ++bus_writes; bus_value = value;
    return BVSTK_OK;
}
static bvstk_status_t transfer(void *ctx, uint8_t addr, const uint8_t *tx, size_t tx_size,
                               uint8_t *rx, size_t rx_size, uint32_t timeout)
{
    (void)ctx; (void)timeout;
    assert(allow_bus && bus_locked && tx_size >= 1);
    last_address = addr; last_register = tx[0];
    if (rx_size) { assert(tx_size == 1 && rx_size == 1); ++bus_reads; *rx = bus_value; }
    else { assert(tx_size == 2); ++bus_writes; bus_value = tx[1]; }
    return BVSTK_OK;
}
static bvstk_status_t lock_bus(void *ctx, uint32_t ms)
{ (void)ctx; (void)ms; assert(!bus_locked); bus_locked = 1; return BVSTK_OK; }
static void unlock_bus(void *ctx)
{ (void)ctx; assert(bus_locked); bus_locked = 0; }
static void setup(bvstk_i2c_master_service_t *service, bvstk_i2c_devices_t *devices,
    bvstk_i2c_cache_t *cache, bvstk_i2c_policy_t *policy, i2c_device_config_t *config)
{
    const bvstk_i2c_master_io_t io = {.read_reg = no_read, .write_reg = no_write};
    memset(config, 0, sizeof(*config));
    strcpy(config->name, "test");
    strcpy(config->file_name, "test.json");
    config->addr_7b = 0x36; config->reg_count = 8; config->max_value_code = 64;
    config->policy = I2C_POLICY_BLACKLIST;
    config->settings_len = 1; config->settings[0] = (i2c_rule_entry_t){1, 0x11};
    assert(bvstk_i2c_devices_init_from_config(devices, config, 1) == BVSTK_OK);
    assert(bvstk_i2c_cache_init(cache, 1) == BVSTK_OK);
    assert(bvstk_i2c_policy_init(policy, config, 1) == BVSTK_OK);
    assert(bvstk_i2c_master_service_init(service, NULL, &io, devices, cache, policy, NULL) == BVSTK_OK);
    assert(bvstk_i2c_cache_write(cache, 0, 1, 0xaa) == BVSTK_OK);
    assert(bvstk_i2c_cache_write(cache, 0, 2, 0xbb) == BVSTK_OK);
    assert(bvstk_i2c_master_service_rule_allow(service, 0, 3, 0x33) == BVSTK_OK);
    assert(bvstk_i2c_master_service_rule_deny(service, 0, 4, 0x24) == BVSTK_OK);
    assert(bvstk_i2c_master_service_set_policy(service, 0, I2C_POLICY_WHITELIST) == BVSTK_OK);
}
static void check_policy(const i2c_device_config_t *config)
{
    assert(config->policy == I2C_POLICY_WHITELIST);
    assert(config->whitelist_len == 1 && config->whitelist[0].reg == 3 && config->whitelist[0].val == 0x33);
    assert(config->blacklist_len == 1 && config->blacklist[0].reg == 4 && config->blacklist[0].val == 0x24);
}
static void check_settings(const i2c_device_config_t *config, int cached)
{
    assert(config->settings_len == (cached ? 2U : 1U));
    assert(config->settings[0].reg == 1 && config->settings[0].val == (cached ? 0xaa : 0x11));
    if (cached) assert(config->settings[1].reg == 2 && config->settings[1].val == 0xbb);
}
static void check_sync(int (*sync)(size_t), size_t id, int expected, unsigned expected_stores)
{
    unsigned entered = critical_enters, exited = critical_exits, stored = store_calls;
    assert(critical_depth == 0);
    assert(sync(id) == expected);
    assert(critical_depth == 0);
    assert(critical_enters == entered + 1 && critical_exits == exited + 1);
    assert(store_calls == stored + expected_stores);
}
static void run_competing_edit(void)
{
    assert(critical_depth == 0);
    ++competing_edits;
    assert(bvstk_i2c_master_service_rule_allow(&s_i2c_master_service, 0, 6, 0x16) == BVSTK_OK);
    assert(bvstk_runtime_i2c_sync_policy(0));
}
static void check_freertos_critical_sections(void)
{
    int (*syncs[])(size_t) = {bvstk_runtime_i2c_sync_device, bvstk_runtime_i2c_sync_policy};
    for (size_t i = 0; i < sizeof(syncs) / sizeof(syncs[0]); ++i) {
        setup(&s_i2c_master_service, &s_i2c_devices, &s_i2c_cache, &s_i2c_policy, &freertos_stored);
        check_sync(syncs[i], 0, 1, 1);
        store_ok = 0;
        check_sync(syncs[i], 0, 0, 1);
        store_ok = 1;
        check_sync(syncs[i], 1, 0, 0);
        store_present = 0;
        check_sync(syncs[i], 0, 0, 0);
        store_present = 1;
        s_i2c_ready = 0;
        check_sync(syncs[i], 0, 0, 0);
        s_i2c_ready = 1;
        s_i2c_policy.initialized = 0;
        check_sync(syncs[i], 0, 0, 0);
        s_i2c_policy.initialized = 1;

        competing_edits = 0;
        schedule_competing_edit = 1;
        unsigned entered = critical_enters, exited = critical_exits, stored = store_calls;
        assert(syncs[i](0));
        assert(critical_depth == 0 && competing_edits == 1 && !competing_edit_pending);
        assert(critical_enters == entered + 2 && critical_exits == exited + 2);
        assert(store_calls == stored + 2);
        assert(freertos_stored.whitelist_len == 2);
        assert(freertos_stored.whitelist[1].reg == 6 && freertos_stored.whitelist[1].val == 0x16);
        bvstk_i2c_policy_entry_t snapshot;
        assert(bvstk_i2c_master_service_get_policy(&s_i2c_master_service, 0, &snapshot) == BVSTK_OK);
        assert(snapshot.whitelist_len == freertos_stored.whitelist_len);
        assert(!memcmp(snapshot.whitelist, freertos_stored.whitelist,
                       snapshot.whitelist_len * sizeof(snapshot.whitelist[0])));
        check_settings(&freertos_stored, i == 0);
    }
    assert(critical_enters == critical_exits && !critical_depth);
}
static void check_explicit_save(void)
{
    save_ok = store_ok = store_ready = 1;
    saves = 0;
    setup(&s_i2c_master_service, &s_i2c_devices, &s_i2c_cache, &s_i2c_policy, &freertos_stored);
    s_i2c_ready = 1;
    strcpy(freertos_stored.profile.register_map, "register_map.json");
    strcpy(freertos_stored.profile.view, "view.js");
    unsigned stored = store_calls;
    assert(bvstk_freertos_dcp2_policy_save(NULL, 2, 0) == BVSTK_OK);
    assert(saves == 1 && store_calls == stored);
    check_policy(&saved); check_settings(&saved, 0);
    assert(!strcmp(saved.profile.register_map, "register_map.json"));
    assert(!strcmp(saved.profile.view, "view.js"));
    save_ok = 0;
    assert(bvstk_freertos_dcp2_policy_save(NULL, 2, 0) == BVSTK_ERR_IO);
    assert(saves == 2);
    store_ready = 0;
    assert(bvstk_freertos_dcp2_policy_save(NULL, 2, 0) == BVSTK_ERR_BUSY);
    assert(saves == 2);
    store_ready = save_ok = 1;
    assert(!bvstk_runtime_i2c_policy_snapshot(0, NULL));
    assert(bvstk_freertos_dcp2_policy_save(NULL, 2, 1) == BVSTK_ERR_INTERNAL);
    assert(saves == 2);
    /* A later edit must not tear the snapshot or be overwritten by saving it. */
    schedule_competing_edit = 1; competing_edits = 0;
    assert(bvstk_freertos_dcp2_policy_save(NULL, 2, 0) == BVSTK_OK);
    assert(saves == 3 && competing_edits == 1);
    check_policy(&saved); check_settings(&saved, 0);
    assert(freertos_stored.whitelist_len == 2);

    memset(&s_mdio_service, 0, sizeof(s_mdio_service));
    s_mdio_service.initialized = 1; s_mdio_service.device_count = 1; s_mdio_ready = 1;
    mdio_phy_config_t *m = &s_mdio_service.devices[0];
    strcpy(m->name, "phy"); m->phy_addr = 1; m->reg_count = 32;
    m->policy = MDIO_POLICY_BLACKLIST;
    m->whitelist_len = 1; m->whitelist[0] = (mdio_rule_entry_t){0, 0x2000, 0x2000};
    m->blacklist_len = 1; m->blacklist[0] = (mdio_rule_entry_t){0, 0x8000, 0x8000};
    m->settings_len = 1; m->settings[0].reg = 4; m->settings[0].val = 0x1e1;
    s_mdio_service.cache[0][4].value = 0x5e1; s_mdio_service.cache[0][4].valid = 1;
    assert(bvstk_freertos_dcp2_policy_save(NULL, 3, 0) == BVSTK_OK);
    assert(saves == 4 && !memcmp(m, &mdio_saved, sizeof(*m)));
    save_ok = 0;
    assert(bvstk_freertos_dcp2_policy_save(NULL, 3, 0) == BVSTK_ERR_IO);
    assert(saves == 5);
    assert(bvstk_freertos_dcp2_policy_save(NULL, 3, 1) != BVSTK_OK);
    assert(saves == 5);
    assert(bvstk_freertos_dcp2_policy_save(NULL, 1, 0) == BVSTK_ERR_UNSUPPORTED);
}

static void check_neutrino_commands(void)
{
    char output[512];
    static i2c_device_config_t before, last_saved;
    memset(&nto, 0, sizeof(nto));
    setup(&nto.master_service, &nto.devices, &nto.cache, &nto.policy, &nto.config_store.devices[0]);
    nto.ready = nto.config_store.initialized = 1;
    nto.config_store.device_count = 1;
    assert(pthread_mutex_init(&nto.state_mutex, NULL) == 0);
    nto.state_mutex_initialized = 1;
    assert(bvstk_i2c_cache_write(&nto.cache, 0, 1, 0x22) == BVSTK_OK);
    assert(bvstk_i2c_cache_write(&nto.cache, 0, 2, 0x33) == BVSTK_OK);
    strcpy(nto.config_store.devices[0].profile.register_map, "register_map.json");
    strcpy(nto.config_store.devices[0].profile.view, "view.js");
    strcpy(nto.config_store.devices[0].profile.icon, "icon.js");
    assert(bvstk_neutrino_i2c_runtime_sync_policy_locked(&nto, 0));
    before = saved = nto.config_store.devices[0];
    store_ok = save_ok = 1; saves = bus_reads = bus_writes = 0; allow_bus = 1;

    assert(!bvstk_neutrino_i2c_runtime_command(&nto,
        "i2c test policy set blacklist", output, sizeof(output)));
    assert(strstr(output, "RAM") && !saves && !bus_reads && !bus_writes);
    assert(!memcmp(&saved, &before, sizeof(saved)));
    assert(!bvstk_neutrino_i2c_runtime_command(&nto,
        "i2c test addr 0x37", output, sizeof(output)));
    assert(nto.config_store.devices[0].addr_7b == 0x37 && !saves && !bus_writes);
    assert(!bvstk_neutrino_i2c_runtime_command(&nto,
        "i2c test w 2 7", output, sizeof(output)));
    assert(bus_writes == 1 && !bus_reads && !saves && bus_value == 7);

    bvstk_control_api_t control = {.i2c = &nto.master_service};
    i2c_device_config_t invalid = nto.config_store.devices[0];
    invalid.addr_7b = 0x38;
    invalid.whitelist_len = I2C_CFG_RULES_MAX + 1U;
    assert(bvstk_control_i2c_apply(&control, 0, &invalid) == BVSTK_ERR_MALFORMED);
    bvstk_i2c_device_t unchanged_device;
    assert(bvstk_i2c_master_service_device_info(control.i2c, 0, &unchanged_device) == BVSTK_OK);
    assert(unchanged_device.addr_7b == 0x37);
    uint8_t body[] = {0x37, 2, 9};
    uint8_t response[64];
    size_t size;
    bvstk_dcp2_request_t request = {BVSTK_DCP2_SERVICE_I2C,
        BVSTK_DCP2_OP_I2C_WRITE_REG, 1, body, sizeof(body)};
    bvstk_neutrino_i2c_runtime_lock(&nto);
    assert(!bvstk_neutrino_dcp2_request_locked(&nto, &control, &request,
        response, sizeof(response), &size));
    assert(!bvstk_dcp2_read_be16(response + 12) && bus_writes == 2 && !saves);
    request.opcode = BVSTK_DCP2_OP_I2C_POLICY_SET;
    request.body_size = 2; body[1] = I2C_POLICY_WHITELIST;
    assert(!bvstk_neutrino_dcp2_request_locked(&nto, &control, &request,
        response, sizeof(response), &size));
    assert(!bvstk_dcp2_read_be16(response + 12) && !saves);
    assert(nto.config_store.devices[0].policy == I2C_POLICY_WHITELIST);
    bvstk_neutrino_i2c_runtime_unlock(&nto);
    assert(!memcmp(&saved, &before, sizeof(saved)));

    assert(!bvstk_neutrino_i2c_runtime_command(&nto, "i2c test save", output, sizeof(output)));
    assert(saves == 1 && strstr(output, "OK saved") && !bus_reads && bus_writes == 2);
    assert(saved.addr_7b == 0x37 && saved.policy == I2C_POLICY_WHITELIST);
    /* Address changes invalidate the previous physical samples. */
    assert(saved.settings_len == 2 && saved.settings[0].val == 0x11 && saved.settings[1].val == 9);
    assert(!strcmp(saved.profile.view, "view.js") && !strcmp(saved.profile.icon, "icon.js"));
    last_saved = saved;
    assert(!bvstk_neutrino_i2c_runtime_command(&nto,
        "i2c test policy whitelist add 2 9", output, sizeof(output)));
    assert(saves == 1 && nto.config_store.devices[0].whitelist_len == 2);
    save_ok = 0;
    assert(bvstk_neutrino_i2c_runtime_command(&nto,
        "i2c test save", output, sizeof(output)) == 1);
    assert(strstr(output, "ERR i2c io") && saves == 2);
    assert(nto.config_store.devices[0].whitelist_len == 2);
    assert(!memcmp(&saved, &last_saved, sizeof(saved)));
    assert(!bus_reads && bus_writes == 2); /* Saving never performs a bus read. */
    /* The same multi-space device accepts a name or an unambiguous wire address. */
    i2c_device_config_t multi = {0};
    strcpy(multi.name, "test"); strcpy(multi.file_name, "test.json");
    multi.addr_7b = 0x50; multi.reg_count = 8; multi.max_value_code = 255;
    multi.policy = I2C_POLICY_BLACKLIST;
    multi.endpoint.transfer.configured = 1; multi.endpoint.transfer.address_bytes = 1;
    multi.endpoint.space_count = 2;
    multi.endpoint.spaces[0] = (bvstk_address_space_t){"id", 0, 4, 0x50};
    multi.endpoint.spaces[1] = (bvstk_address_space_t){"diag", 4, 4, 0x51};
    nto.config_store.devices[0] = multi;
    const bvstk_i2c_master_io_t io = {.transfer = transfer, .lock = lock_bus, .unlock = unlock_bus};
    assert(bvstk_i2c_devices_init_from_config(&nto.devices, &multi, 1) == BVSTK_OK);
    assert(bvstk_i2c_cache_init(&nto.cache, 1) == BVSTK_OK);
    assert(bvstk_i2c_policy_init(&nto.policy, &multi, 1) == BVSTK_OK);
    assert(bvstk_i2c_master_service_init(&nto.master_service, NULL, &io, &nto.devices,
        &nto.cache, &nto.policy, NULL) == BVSTK_OK);
    bus_value = 0x2a;
    assert(!bvstk_neutrino_i2c_runtime_command(&nto, "i2c @0x51 r 1", output, sizeof(output)));
    assert(last_address == 0x51 && last_register == 1 && strstr(output, "0x2A"));
    assert(!bvstk_neutrino_i2c_runtime_command(&nto, "i2c @0x51 w 1 7", output, sizeof(output)));
    assert(last_address == 0x51 && last_register == 1 && bus_value == 7);
    assert(!bus_locked);
    assert(bvstk_neutrino_i2c_runtime_command(&nto, "i2c @0x51 r diag:1", output, sizeof(output)) == 2);
    assert(bvstk_neutrino_i2c_runtime_command(&nto, "i2c @0x52 r 1", output, sizeof(output)) == 1);
    assert(saves == 2);
    allow_bus = 0; save_ok = 1;
    assert(pthread_mutex_destroy(&nto.state_mutex) == 0);
}

int main(void)
{
    (void)s_spi_mutex; /* SPI is disabled in this board configuration. */
    char output[256];
    setup(&s_i2c_master_service, &s_i2c_devices, &s_i2c_cache, &s_i2c_policy, &freertos_stored);
    s_i2c_ready = 1;
    check_sync(bvstk_runtime_i2c_sync_policy, 0, 1, 1);
    check_policy(&freertos_stored); check_settings(&freertos_stored, 0);
    assert(saves == 0);
    check_sync(bvstk_runtime_i2c_sync_device, 0, 1, 1);
    check_policy(&freertos_stored); check_settings(&freertos_stored, 1);
    assert(saves == 0);
    check_freertos_critical_sections();

    setup(&nto.master_service, &nto.devices, &nto.cache, &nto.policy, &nto.config_store.devices[0]);
    nto.ready = 1; nto.config_store.initialized = 1; nto.config_store.device_count = 1;
    assert(pthread_mutex_init(&nto.state_mutex, NULL) == 0);
    nto.state_mutex_initialized = 1;
    bvstk_neutrino_i2c_runtime_lock(&nto);
    assert(bvstk_neutrino_i2c_runtime_sync_policy_locked(&nto, 0));
    check_policy(&nto.config_store.devices[0]); check_settings(&nto.config_store.devices[0], 0);
    assert(saves == 0);
    /* Actual console edit starts from the mirror and keeps every rule in RAM. */
    bvstk_neutrino_i2c_runtime_unlock(&nto);
    assert(!bvstk_neutrino_i2c_runtime_command(&nto,
        "i2c test policy blacklist add 5 37", output, sizeof(output)));
    bvstk_neutrino_i2c_runtime_lock(&nto);
    assert(strstr(output, "RAM") && saves == 0);
    assert(nto.config_store.devices[0].whitelist_len == 1 && nto.config_store.devices[0].whitelist[0].val == 0x33);
    assert(nto.config_store.devices[0].blacklist_len == 2 &&
           nto.config_store.devices[0].blacklist[0].val == 0x24 &&
           nto.config_store.devices[0].blacklist[1].val == 37);
    check_settings(&nto.config_store.devices[0], 0);
    assert(bvstk_neutrino_i2c_runtime_save_device_locked(&nto, 0));
    assert(saves == 1); check_settings(&saved, 1);
    save_ok = 0;
    assert(bvstk_neutrino_i2c_runtime_sync_policy_locked(&nto, 0));
    assert(saves == 1);
    assert(!bvstk_neutrino_i2c_runtime_save_device_locked(&nto, 0));
    assert(saves == 2);
    store_ok = 0;
    assert(!bvstk_neutrino_i2c_runtime_sync_policy_locked(&nto, 0));
    store_ok = save_ok = 1;
    setup(&nto.master_service, &nto.devices, &nto.cache, &nto.policy, &nto.config_store.devices[0]);
    assert(bvstk_neutrino_i2c_runtime_save_policy_locked(&nto, 0));
    assert(saves == 3); check_policy(&saved); check_settings(&saved, 0);
    save_ok = 0;
    assert(!bvstk_neutrino_i2c_runtime_save_policy_locked(&nto, 0));
    assert(saves == 4);
    bvstk_neutrino_i2c_runtime_unlock(&nto);
    assert(pthread_mutex_destroy(&nto.state_mutex) == 0);
    check_explicit_save();
    check_neutrino_commands();
    puts("DCP2 policy runtime synchronization tests passed");
    return 0;
}
