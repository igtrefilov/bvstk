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
#define update_setting neutrino_update_setting
#include "apps/neutrino/runtime/bvstk_i2c_runtime.c"
#undef update_setting

static i2c_device_config_t freertos_stored, saved;
static unsigned saves;
static int store_ok = 1, save_ok = 1, store_present = 1;
static bvstk_neutrino_i2c_runtime_t nto;

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
    assert(!"unexpected bus read");
    return BVSTK_ERR_IO;
}
static bvstk_status_t no_write(void *ctx, uint8_t addr, uint8_t reg, uint8_t value, uint32_t timeout)
{
    (void)ctx; (void)addr; (void)reg; (void)value; (void)timeout;
    assert(!"unexpected bus write");
    return BVSTK_ERR_IO;
}
static void setup(bvstk_i2c_master_service_t *service, bvstk_i2c_devices_t *devices,
    bvstk_i2c_cache_t *cache, bvstk_i2c_policy_t *policy, i2c_device_config_t *config)
{
    const bvstk_i2c_master_io_t io = {NULL, no_read, no_write};
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
int main(void)
{
    (void)s_spi_mutex; /* SPI is disabled in this board configuration. */
    char output[256];
    char *args[] = {"test", "policy", "blacklist", "add", "5", "37"};
    response_writer_t writer = {output, sizeof(output), 0, 0};
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
    /* Actual console edit starts from the mirror and persists all DCP2 rules. */
    assert(policy_edit(&nto, 0, &nto.config_store.devices[0], args, 6, &writer) == 0);
    assert(strstr(output, "OK") && saves == 1);
    assert(saved.whitelist_len == 1 && saved.whitelist[0].val == 0x33);
    assert(saved.blacklist_len == 2 && saved.blacklist[0].val == 0x24 && saved.blacklist[1].val == 37);
    check_settings(&saved, 0);
    assert(bvstk_neutrino_i2c_runtime_sync_locked(&nto, 0, 1));
    assert(saves == 2); check_settings(&saved, 1);
    save_ok = 0;
    assert(bvstk_neutrino_i2c_runtime_sync_policy_locked(&nto, 0));
    assert(saves == 2);
    assert(!bvstk_neutrino_i2c_runtime_sync_locked(&nto, 0, 1));
    assert(saves == 3);
    store_ok = 0;
    assert(!bvstk_neutrino_i2c_runtime_sync_policy_locked(&nto, 0));
    bvstk_neutrino_i2c_runtime_unlock(&nto);
    assert(pthread_mutex_destroy(&nto.state_mutex) == 0);
    puts("DCP2 policy runtime synchronization tests passed");
    return 0;
}
