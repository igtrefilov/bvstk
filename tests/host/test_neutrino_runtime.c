#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "apps/neutrino/bvstkd/bvstk_dcp2_runtime.h"
#include "shared/config/bvstk_device_profile.h"
#include "shared/config/bvstk_i2c_config_codec.h"
#include "shared/config/bvstk_mdio_config_codec.h"
#include "ports/neutrino-zynq7000/os/bvstk_observation_neutrino.h"
#include "apps/neutrino/observe/bvstk_observe.h"

static bvstk_neutrino_i2c_runtime_t i2c;
static bvstk_neutrino_mdio_runtime_t mdio;
static bvstk_dcp2_policy_session_t session;
static bvstk_neutrino_dcp2_runtime_t runtime = {&i2c, &mdio};
static unsigned i2c_reads, mdio_reads, mdio_writes;
static int i2c_connected = 1, mdio_connected = 1;
static uint16_t registers[32];
static uint8_t response[BVSTK_DCP2_MAX_PAYLOAD + 8];
static bvstk_observation_neutrino_t observation;
static bvstk_cache_snapshot_t snapshot;
static bvstk_guard_t original_guard;
static _Thread_local unsigned catalog_held;

static void catalog_enter(void *context)
{
    (void)context;
    original_guard.enter(original_guard.context);
    ++catalog_held;
}
static void catalog_leave(void *context)
{
    (void)context;
    assert(catalog_held);
    --catalog_held;
    original_guard.leave(original_guard.context);
}

static void cached(const char *bus, const char *name)
{
    unsigned i = i2c_reads, m = mdio_reads;
    uint16_t handle;
    uint64_t cursor;
    assert(bvstk_cache_snapshot(&observation.cache, bus, name, &snapshot,
        &handle, &cursor, bvstk_platform_now_ms()) == BVSTK_OK);
    assert(i == i2c_reads && m == mdio_reads);
}

uint64_t bvstk_platform_now_ms(void)
{
    struct timespec now;
    assert(!clock_gettime(CLOCK_MONOTONIC, &now));
    return (uint64_t)now.tv_sec * 1000U + (unsigned long)now.tv_nsec / 1000000U;
}
void bvstk_platform_sleep_ms(uint32_t ms)
{
    struct timespec delay = {ms / 1000U, (long)(ms % 1000U) * 1000000L};
    nanosleep(&delay, NULL);
}
int bvstk_mmio_region_open(bvstk_mmio_region_t *r, uintptr_t a, size_t s)
{ (void)r; (void)a; (void)s; assert(!"unexpected MMIO map"); return -1; }
void bvstk_mmio_region_close(bvstk_mmio_region_t *r) { (void)r; }
int bvstk_mmio_read32(const bvstk_mmio_region_t *r, size_t o, uint32_t *v)
{ (void)r; (void)o; (void)v; assert(!"unexpected MMIO read"); return -1; }
int bvstk_mmio_write32(const bvstk_mmio_region_t *r, size_t o, uint32_t v)
{ (void)r; (void)o; (void)v; assert(!"unexpected MMIO write"); return -1; }
bvstk_status_t bvstk_i2c_slave_hw_set_address(bvstk_i2c_slave_hw_t *h, uint8_t a)
{ (void)h; (void)a; assert(!"unexpected slave address change"); return BVSTK_ERR_IO; }
size_t bvstk_i2c_slave_service_target(const bvstk_i2c_slave_service_t *s)
{ (void)s; assert(!"unexpected slave target query"); return 0; }

static void read_json(const char *path, char *out, size_t cap)
{
    FILE *f = fopen(path, "rb"); assert(f);
    size_t n = fread(out, 1, cap - 1U, f);
    assert(!ferror(f) && feof(f) && !fclose(f)); out[n] = 0;
}
static void write_json(const char *path, const char *data)
{
    FILE *f = fopen(path, "wb"); assert(f);
    assert(fputs(data, f) >= 0 && !fclose(f));
}
static bvstk_status_t i2c_lock(void *c, uint32_t ms)
{ (void)c; (void)ms; return BVSTK_OK; }
static void i2c_unlock(void *c) { (void)c; }
static bvstk_status_t i2c_transfer(void *c, uint8_t addr, const uint8_t *tx,
    size_t tx_size, uint8_t *rx, size_t rx_size, uint32_t timeout)
{
    assert(!catalog_held);
    (void)c; (void)timeout;
    assert(addr == 0x57 && tx_size == 1 && rx && rx_size && rx_size <= BVSTK_ACQUISITION_CHUNK_MAX);
    ++i2c_reads;
    if (!i2c_connected) return BVSTK_ERR_IO;
    for (size_t j = 0; j < rx_size; ++j) rx[j] = (uint8_t)(tx[0] + j + 1U);
    return BVSTK_OK;
}
static bvstk_status_t mdio_read(void *c, uint8_t phy, uint8_t reg, uint16_t *value, uint32_t timeout)
{
    assert(!catalog_held);
    (void)c; (void)timeout; assert(phy == 1 && reg < 32U);
    ++mdio_reads; *value = mdio_connected ? registers[reg] : 0xFFFFU;
    return BVSTK_OK;
}
static bvstk_status_t mdio_write(void *c, uint8_t phy, uint8_t reg, uint16_t value,
    bvstk_event_source_t source, uint32_t timeout)
{
    assert(!catalog_held);
    (void)c; (void)source; (void)timeout; assert(phy == 1 && reg < 32U);
    ++mdio_writes; registers[reg] = value; return BVSTK_OK;
}
static bvstk_status_t mdio_publish(void *c, uint8_t phy, uint8_t reg, uint16_t value)
{ assert(!catalog_held); (void)c; (void)phy; (void)reg; (void)value; return BVSTK_OK; }

static uint16_t dcp(uint8_t service, uint8_t opcode, const uint8_t *body, uint16_t size,
    int i2c_ready, int mdio_ready)
{
    bvstk_control_api_t control = {.i2c = i2c_ready ? &i2c.master_service : NULL,
        .mdio = mdio_ready ? &mdio.service : NULL};
    bvstk_dcp2_request_t request = {service, opcode, 1U, body, size};
    size_t received = 0;
    assert(!bvstk_neutrino_dcp2_dispatch(&runtime, &session, &control,
        &request, response, sizeof(response), &received));
    assert(received >= 14U);
    return bvstk_dcp2_read_be16(response + 12U);
}
static size_t policy_header(uint8_t *body)
{
    body[0] = 1; body[1] = 4; memcpy(body + 2, "mdio", 4);
    body[6] = 7; memcpy(body + 7, "lan8720", 7);
    return 14;
}
static void poll_i2c(uint64_t now)
{
    bvstk_neutrino_i2c_runtime_lock(&i2c);
    bvstk_neutrino_i2c_runtime_poll_locked(&i2c, now);
    bvstk_neutrino_i2c_runtime_unlock(&i2c);
}
static void poll_mdio(uint64_t now)
{
    bvstk_neutrino_mdio_runtime_lock(&mdio);
    bvstk_neutrino_mdio_runtime_poll_locked(&mdio, now);
    bvstk_neutrino_mdio_runtime_unlock(&mdio);
}
int main(int argc, char **argv)
{
    assert(argc == 3 && !mkdir(argv[1], 0700));
    assert(bvstk_observation_neutrino_init(&observation));
    original_guard = observation.cache.guard;
    observation.cache.guard = (bvstk_guard_t){NULL, catalog_enter, catalog_leave};
    const bvstk_event_sink_t events = {&observation.events, bvstk_observation_publish};
    static char json[BVSTK_PROFILE_JSON_MAX + 1U], original[BVSTK_PROFILE_JSON_MAX + 1U];
    char path[512], directory[512];
    assert(snprintf(path, sizeof(path), "%s/configs/i2c/pca9545a/p24c02a/p24c02a.json", argv[2]) < (int)sizeof(path));
    read_json(path, json, sizeof(json));
    i2c_device_config_t *ic = i2c.config_store.devices;
    assert(bvstk_i2c_config_parse_json(json, ic) == BVSTK_OK);
    ic->endpoint.parent[0] = 0; /* Direct recording backend; routing has separate tests. */
    i2c.config_store.device_count = 1; i2c.config_store.initialized = 1;
    assert(bvstk_i2c_devices_init_from_config(&i2c.devices, ic, 1) == BVSTK_OK);
    assert(bvstk_i2c_cache_init(&i2c.cache, 1) == BVSTK_OK);
    assert(bvstk_i2c_policy_init(&i2c.policy, ic, 1) == BVSTK_OK);
    const bvstk_i2c_master_io_t io = {.transfer = i2c_transfer, .lock = i2c_lock, .unlock = i2c_unlock};
    assert(bvstk_i2c_master_service_init(&i2c.master_service, NULL, &io,
        &i2c.devices, &i2c.cache, &i2c.policy, &events) == BVSTK_OK);
    assert(!pthread_mutex_init(&i2c.state_mutex, NULL));
    i2c.state_mutex_initialized = i2c.ready = 1;
    assert(bvstk_i2c_cache_seed(&i2c.cache, 0, 0, 0xAA) == BVSTK_OK);
    assert(bvstk_cache_attach_i2c(&i2c.cache_bridge, &observation.cache, &i2c.master_service, ic) == BVSTK_OK);
    cached("i2c", "p24c02a");
    assert(snapshot.presence == BVSTK_CACHE_PRESENCE_UNKNOWN && snapshot.entries[0].valid);
    assert(snapshot.entries[0].value == 0xAA && snapshot.entries[0].source == BVSTK_CACHE_SOURCE_SETTINGS);
    poll_i2c(0);
    bvstk_i2c_presence_t presence;
    assert(bvstk_i2c_master_service_device_presence(&i2c.master_service, 0, &presence) == BVSTK_OK);
    assert(presence == BVSTK_I2C_PRESENCE_RESPONDING && i2c.probe_count[0] == 1);
    for (uint64_t now = 10; now < 5000; now += 10) poll_i2c(now);
    assert(i2c.acquisition_count[0] >= 256);
    cached("i2c", "p24c02a");
    assert(snapshot.presence == BVSTK_CACHE_PRESENCE_READY && snapshot.count == 256);
    for (unsigned r = 0; r < snapshot.count; ++r)
        assert(snapshot.entries[r].valid && snapshot.entries[r].source == BVSTK_CACHE_SOURCE_DEVICE);
    assert(!bvstk_observation_head(&observation.events, 0, BVSTK_JOURNAL_TELEMETRY));
    for (unsigned r = 0; r < 256; ++r) {
        bvstk_i2c_cache_entry_t e;
        assert(bvstk_i2c_master_service_cache_entry(&i2c.master_service, 0, (uint8_t)r, &e) == BVSTK_OK);
        assert(e.valid && !e.seeded && e.value == (uint8_t)(r + 1U));
    }
    i2c_connected = 0;
    poll_i2c(5000); poll_i2c(5010);
    assert(bvstk_i2c_master_service_device_presence(&i2c.master_service, 0, &presence) == BVSTK_OK);
    assert(presence == BVSTK_I2C_PRESENCE_UNAVAILABLE && !i2c.acquisition[0].connected);
    cached("i2c", "p24c02a");
    assert(snapshot.presence == BVSTK_CACHE_PRESENCE_UNAVAILABLE);
    for (unsigned r = 0; r < snapshot.count; ++r) assert(!snapshot.entries[r].valid);
    i2c_connected = 1; poll_i2c(5250); poll_i2c(5260);
    assert(i2c.acquisition[0].connected);
    unsigned previous = i2c_reads;
    ic->presence_check_enabled = 0; i2c.devices.items[0].presence_check_enabled = 0;
    ic->profile.acquisition.enabled = 0;
    for (uint64_t now = 6000; now < 7000; now += 10) poll_i2c(now);
    assert(previous == i2c_reads); /* Disabled jobs do not scan the bus. */

    assert(snprintf(path, sizeof(path), "%s/configs/mdio/lan8720/lan8720.json", argv[2]) < (int)sizeof(path));
    read_json(path, json, sizeof(json));
    mdio_phy_config_t mc;
    assert(bvstk_mdio_config_parse(json, &mc) == BVSTK_OK);
    strcpy(mc.file_name, "lan8720/lan8720.json");
    mc.whitelist_len = mc.blacklist_len = 0; mc.policy = MDIO_POLICY_WHITELIST;
    mc.settings[0] = (mdio_setting_entry_t){1, 0x0022}; mc.settings_len = 1;
    mc.autopoll_enabled = 0;
    mc.profile.acquisition.enabled = 1; mc.profile.acquisition.group_count = 1;
    mc.profile.acquisition.read_delay_ms = 10; mc.profile.acquisition.retry_delay_ms = 100;
    bvstk_acquisition_group_t *group = &mc.profile.acquisition.groups[0];
    strcpy(group->id, "sample"); group->chunk_size = 1; group->range_count = 1;
    group->ranges[0].first = 1; group->ranges[0].count = 2; group->interval_ms = 100;
    assert(snprintf(directory, sizeof(directory), "%s/mdio", argv[1]) < (int)sizeof(directory));
    strcpy(mdio.config_store.primary_dir, directory); mdio.config_store.initialized = 1;
    assert(bvstk_neutrino_mdio_config_store_save(&mdio.config_store, &mc));
    assert(bvstk_neutrino_mdio_config_store_init(&mdio.config_store, directory, "/missing-legacy"));
    assert(mdio.config_store.device_count == 1 && mdio.config_store.devices[0].profile.acquisition.enabled);
    assert(snprintf(path, sizeof(path), "%s/lan8720/lan8720.json", directory) < (int)sizeof(path));
    read_json(path, original, sizeof(original));
    registers[2] = 0x0007; registers[3] = 0xC0F1; registers[1] = 0x1234;
    const bvstk_mdio_bus_ops_t bus = {.read = mdio_read, .write = mdio_write, .publish = mdio_publish};
    assert(bvstk_mdio_service_init(&mdio.service, NULL, &bus,
        mdio.config_store.devices, 1, &events) == BVSTK_OK);
    assert(!pthread_mutex_init(&mdio.state_mutex, NULL)); mdio.state_initialized = mdio.ready = 1;
    assert(bvstk_neutrino_mutex_init(&mdio.service_mutex) == BVSTK_OK);
    mdio.service_mutex_initialized = 1; mdio.service.mutex = mdio.service_mutex.public_mutex;
    assert(bvstk_cache_attach_mdio(&mdio.cache_bridge, &observation.cache, &mdio.service) == BVSTK_OK);
    cached("mdio", "lan8720");
    assert(snapshot.entries[1].source == BVSTK_CACHE_SOURCE_SETTINGS && snapshot.entries[1].value == 0x22);
    for (uint64_t now = 0; now < 100; now += 10) poll_mdio(now);
    assert(bvstk_mdio_service_ready(&mdio.service, 0) == 1 && mdio.acquisition_count[0] >= 2);
    bvstk_mdio_cache_entry_t e;
    assert(bvstk_mdio_service_cache_entry(&mdio.service, 0, 1, &e) == BVSTK_OK);
    assert(e.valid && !e.seeded && e.value == 0x1234);
    cached("mdio", "lan8720");
    assert(snapshot.presence == BVSTK_CACHE_PRESENCE_READY && snapshot.entries[1].value == 0x1234);
    assert(snapshot.entries[1].source == BVSTK_CACHE_SOURCE_DEVICE);
    mdio_connected = 0; poll_mdio(1000); poll_mdio(1010);
    assert(!bvstk_mdio_service_ready(&mdio.service, 0) && !mdio.acquisition[0].connected);
    cached("mdio", "lan8720");
    assert(snapshot.presence == BVSTK_CACHE_PRESENCE_UNAVAILABLE);
    for (unsigned r = 0; r < snapshot.count; ++r) assert(!snapshot.entries[r].valid);
    mdio_connected = 1; poll_mdio(2000); poll_mdio(2010);
    assert(bvstk_mdio_service_ready(&mdio.service, 0) == 1 && mdio.acquisition[0].connected);

    uint8_t write[] = {1, 4, 0, 0xA5}, mode[] = {1, MDIO_POLICY_BLACKLIST};
    assert(dcp(BVSTK_DCP2_SERVICE_MDIO, BVSTK_DCP2_OP_MDIO_WRITE, write, sizeof(write), 1, 1));
    assert(!mdio_writes);
    assert(!dcp(BVSTK_DCP2_SERVICE_MDIO, BVSTK_DCP2_OP_MDIO_POLICY_SET, mode, sizeof(mode), 1, 1));
    assert(!dcp(BVSTK_DCP2_SERVICE_MDIO, BVSTK_DCP2_OP_MDIO_WRITE, write, sizeof(write), 1, 1));
    assert(mdio_writes == 1 && registers[4] == 0xA5);
    cached("mdio", "lan8720");
    assert(snapshot.entries[4].valid && snapshot.entries[4].value == 0xA5);
    bvstk_observe_message_t query = {.version = BVSTK_OBSERVE_VERSION,
        .bus = BVSTK_EVENT_BUS_MDIO, .channel = BVSTK_JOURNAL_EVENTS, .address = -1,
        .after = BVSTK_JOURNAL_HISTORY, .device = "lan8720"};
    previous = mdio_reads;
    bvstk_neutrino_observe_query(&observation.events, &runtime, &query);
    assert(query.status == BVSTK_OK && query.batch.count == 4 && mdio_reads == previous);
    assert(query.batch.records[1].event.type == BVSTK_EVENT_REG_DENIED);
    assert(query.batch.records[3].event.type == BVSTK_EVENT_REG_COMMIT);
    query.channel = BVSTK_JOURNAL_TELEMETRY;
    bvstk_neutrino_observe_query(&observation.events, &runtime, &query);
    assert(query.status == BVSTK_OK && query.batch.count == 2);
    for (size_t r = 0; r < query.batch.count; ++r)
        assert(query.batch.records[r].event.source == BVSTK_EVENT_SOURCE_DCP);
    uint8_t read_i2c[] = {0x57, 0};
    assert(!dcp(BVSTK_DCP2_SERVICE_I2C, BVSTK_DCP2_OP_I2C_READ_REG, read_i2c, 2, 1, 1));
    query.bus = BVSTK_EVENT_BUS_I2C;
    strcpy(query.device, "p24c02a");
    bvstk_neutrino_observe_query(&observation.events, &runtime, &query);
    assert(query.status == BVSTK_OK && query.batch.count == 1);
    assert(query.batch.records[0].event.source == BVSTK_EVENT_SOURCE_DCP);
    char local[1024];
    assert(!bvstk_neutrino_i2c_runtime_command(&i2c, "i2c p24c02a r 0", local, sizeof(local)));
    bvstk_neutrino_observe_query(&observation.events, &runtime, &query);
    assert(query.batch.count == 2 && query.batch.records[1].event.source == BVSTK_EVENT_SOURCE_CONSOLE);
    query.bus = BVSTK_JOURNAL_BUSES;
    bvstk_neutrino_observe_query(&observation.events, &runtime, &query);
    assert(query.status == BVSTK_ERR_MALFORMED && !query.batch.count);
    query.bus = BVSTK_EVENT_BUS_I2C;
    strcpy(query.device, "unknown");
    bvstk_neutrino_observe_query(&observation.events, &runtime, &query);
    assert(query.status == BVSTK_ERR_NOT_FOUND && !query.batch.count);
    read_json(path, json, sizeof(json)); assert(!strcmp(original, json));
    uint8_t named[96] = {0}; size_t n = policy_header(named);
    memset(named + n, 0, 6);
    assert(!dcp(BVSTK_DCP2_SERVICE_POLICY, BVSTK_DCP2_OP_I2C_POLICY_GET, named, (uint16_t)(n + 6), 1, 1));
    assert(response[14] == 1 && response[15] == MDIO_POLICY_BLACKLIST);
    assert(!dcp(BVSTK_DCP2_SERVICE_POLICY, BVSTK_DCP2_OP_I2C_POLICY_SAVE, named, (uint16_t)n, 1, 1));
    static bvstk_neutrino_mdio_config_store_t reloaded;
    assert(bvstk_neutrino_mdio_config_store_init(&reloaded, directory, "/missing-legacy"));
    assert(reloaded.devices[0].policy == MDIO_POLICY_BLACKLIST);
    assert(reloaded.devices[0].settings[0].val == 0x0022); /* SAVE never captures telemetry. */
    assert(!dcp(BVSTK_DCP2_SERVICE_MDIO, BVSTK_DCP2_OP_MDIO_READ, write, 2, 0, 1));
    assert(!dcp(BVSTK_DCP2_SERVICE_PING, BVSTK_DCP2_OP_PING, NULL, 0, 0, 0));
    assert(dcp(BVSTK_DCP2_SERVICE_MDIO, BVSTK_DCP2_OP_MDIO_READ, write, 2, 1, 0) ==
        bvstk_dcp2_status_from_bvstk(BVSTK_ERR_NOT_READY));

    /* A held I2C state lock must not delay the independent MDIO worker. */
    pthread_mutex_lock(&i2c.state_mutex);
    previous = mdio_reads;
    assert(bvstk_neutrino_i2c_runtime_start_background(&i2c));
    assert(bvstk_neutrino_mdio_runtime_start_background(&mdio));
    assert(!bvstk_neutrino_mdio_runtime_start_background(&mdio));
    bvstk_platform_sleep_ms(100);
    bvstk_neutrino_worker_stop(&i2c.worker); /* trylock allows join while I2C is busy. */
    pthread_mutex_unlock(&i2c.state_mutex);
    bvstk_neutrino_worker_stop(&mdio.worker);
    assert(mdio_reads > previous);
    bvstk_neutrino_mdio_runtime_shutdown(&mdio);
    uint16_t unused;
    uint64_t unused_cursor;
    assert(bvstk_cache_snapshot(&observation.cache, "mdio", "lan8720", &snapshot,
        &unused, &unused_cursor, 0) == BVSTK_ERR_NOT_FOUND);
    bvstk_cache_detach_i2c(&i2c.cache_bridge);
    bvstk_observation_neutrino_destroy(&observation);
    pthread_mutex_destroy(&i2c.state_mutex);

    char bad[512]; assert(snprintf(bad, sizeof(bad), "%s/bad.json", directory) < (int)sizeof(bad));
    write_json(bad, "{ invalid");
    assert(!bvstk_neutrino_mdio_config_store_init(&reloaded, directory, "/missing-legacy"));
    assert(!reloaded.initialized && !reloaded.device_count);
    puts("Neutrino runtime: real profiles, background I2C/MDIO acquisition, loss/reconnect, DCP2, explicit SAVE/seeds, independent workers and invalid-profile isolation passed");
    return 0;
}
