/* Real CLI/DCP2 runtime with POSIX profile storage and a recording I2C backend.
 * Reconstructing the runtime from disk models a process restart without PL. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "apps/neutrino/runtime/bvstk_i2c_runtime.h"
#include "apps/neutrino/bvstkd/bvstk_dcp2_runtime.h"
#include "shared/config/bvstk_i2c_config_codec.h"
#include "shared/config/bvstk_device_profile.h"

static bvstk_neutrino_i2c_runtime_t runtime;
static unsigned reads, writes;
static uint8_t bus_value;
uint64_t bvstk_platform_now_ms(void) { return 1000U; }
void bvstk_platform_sleep_ms(uint32_t ms) { (void)ms; }
int bvstk_mmio_region_open(bvstk_mmio_region_t *r, uintptr_t a, size_t size)
{ (void)r; (void)a; (void)size; assert(!"unexpected MMIO map"); return -1; }
void bvstk_mmio_region_close(bvstk_mmio_region_t *r) { (void)r; }
int bvstk_mmio_read32(const bvstk_mmio_region_t *r, size_t o, uint32_t *v)
{ (void)r; (void)o; (void)v; assert(!"unexpected MMIO read"); return -1; }
int bvstk_mmio_write32(const bvstk_mmio_region_t *r, size_t o, uint32_t v)
{ (void)r; (void)o; (void)v; assert(!"unexpected MMIO write"); return -1; }
bvstk_status_t bvstk_i2c_slave_hw_set_address(bvstk_i2c_slave_hw_t *hw, uint8_t a)
{ (void)hw; (void)a; assert(!"unexpected slave address change"); return BVSTK_ERR_IO; }
size_t bvstk_i2c_slave_service_target(const bvstk_i2c_slave_service_t *service)
{ (void)service; assert(!"unexpected slave target query"); return 0; }

static bvstk_status_t read_reg(void *ctx, uint8_t a, uint8_t r, uint8_t *v, uint32_t ms)
{ (void)ctx; (void)a; (void)r; (void)ms; ++reads; *v = bus_value; return BVSTK_OK; }
static bvstk_status_t write_reg(void *ctx, uint8_t a, uint8_t r, uint8_t v, uint32_t ms)
{ (void)ctx; (void)a; (void)r; (void)ms; ++writes; bus_value = v; return BVSTK_OK; }

static void restart(const char *directory)
{
    if (runtime.state_mutex_initialized) assert(!pthread_mutex_destroy(&runtime.state_mutex));
    memset(&runtime, 0, sizeof(runtime));
    assert(bvstk_neutrino_i2c_config_store_init(&runtime.config_store, directory, "/missing-legacy"));
    assert(runtime.config_store.device_count == 1U);
    const i2c_device_config_t *config = runtime.config_store.devices;
    const bvstk_i2c_master_io_t io = {.read_reg = read_reg, .write_reg = write_reg};
    assert(bvstk_i2c_devices_init_from_config(&runtime.devices, config, 1U) == BVSTK_OK);
    assert(bvstk_i2c_cache_init(&runtime.cache, 1U) == BVSTK_OK);
    assert(bvstk_i2c_policy_init(&runtime.policy, config, 1U) == BVSTK_OK);
    for (size_t i = 0; i < config->settings_len; ++i)
        assert(bvstk_i2c_cache_seed(&runtime.cache, 0, config->settings[i].reg,
                                    config->settings[i].val) == BVSTK_OK);
    assert(bvstk_i2c_master_service_init(&runtime.master_service, NULL, &io,
        &runtime.devices, &runtime.cache, &runtime.policy, NULL) == BVSTK_OK);
    assert(!pthread_mutex_init(&runtime.state_mutex, NULL));
    runtime.state_mutex_initialized = runtime.ready = 1;
}
static void command(const char *text, int result)
{
    char response[512];
    assert(bvstk_neutrino_i2c_runtime_command(&runtime, text, response, sizeof(response)) == result);
    assert(strstr(response, result ? "ERR" : "OK"));
}
static void dcp(unsigned opcode, uint8_t *body, uint16_t size)
{
    bvstk_control_api_t control = {.i2c = &runtime.master_service};
    bvstk_dcp2_request_t request = {BVSTK_DCP2_SERVICE_I2C, (uint8_t)opcode, 1, body, size};
    uint8_t response[64]; size_t response_size;
    bvstk_neutrino_i2c_runtime_lock(&runtime);
    assert(!bvstk_neutrino_dcp2_request_locked(&runtime, &control, &request,
        response, sizeof(response), &response_size));
    assert(response_size >= 14 && !bvstk_dcp2_read_be16(response + 12));
    bvstk_neutrino_i2c_runtime_unlock(&runtime);
}
static size_t contents(const char *path, char *buffer, size_t capacity)
{
    FILE *f = fopen(path, "rb"); assert(f);
    size_t n = fread(buffer, 1, capacity, f);
    assert(n < capacity && !ferror(f) && feof(f));
    assert(!fclose(f)); buffer[n] = 0;
    return n;
}
static void unchanged(const char *path, const char *previous, size_t length)
{
    static char current[BVSTK_PROFILE_JSON_MAX + 1U];
    assert(contents(path, current, sizeof(current)) == length && !memcmp(current, previous, length));
}
static uint8_t seed(unsigned reg)
{
    const i2c_device_config_t *c = runtime.config_store.devices;
    for (size_t i = 0; i < c->settings_len; ++i)
        if (c->settings[i].reg == reg) return c->settings[i].val;
    assert(!"missing cache seed"); return 0;
}
int main(int argc, char **argv)
{
    assert(argc == 2 && !mkdir(argv[1], 0700));
    char path[512];
    assert(snprintf(path, sizeof(path), "%s/unit.json", argv[1]) < (int)sizeof(path));
    i2c_device_config_t initial = {0};
    strcpy(initial.name, "unit"); initial.addr_7b = 0x36;
    initial.reg_count = 8; initial.max_value_code = 255; initial.policy = I2C_POLICY_WHITELIST;
    initial.settings[0] = (i2c_rule_entry_t){0, 0x11}; initial.settings_len = 1;
    static char before[BVSTK_PROFILE_JSON_MAX + 1U]; size_t n;
    assert(bvstk_i2c_config_serialize_json(&initial, before, sizeof(before), &n) == BVSTK_OK);
    FILE *f = fopen(path, "wb"); assert(f && fwrite(before, 1, n, f) == n && !fclose(f));
    restart(argv[1]);
    n = contents(path, before, sizeof(before));

    command("i2c unit policy whitelist add 2 7", 0);
    command("i2c unit w 2 7", 0);
    assert(!reads && writes == 1U);
    uint8_t mode[] = {0x36, I2C_POLICY_BLACKLIST}, value[] = {0x36, 2, 9};
    dcp(BVSTK_DCP2_OP_I2C_POLICY_SET, mode, sizeof(mode));
    dcp(BVSTK_DCP2_OP_I2C_WRITE_REG, value, sizeof(value));
    assert(!reads && writes == 2U);
    unchanged(path, before, n);
    restart(argv[1]);
    assert(runtime.config_store.devices[0].policy == I2C_POLICY_WHITELIST);
    assert(runtime.config_store.devices[0].whitelist_len == 0U && seed(0) == 0x11);
    command("i2c unit w 2 7", 1);
    assert(writes == 2U); /* The unsaved permission did not survive restart. */

    command("i2c unit policy whitelist add 2 7", 0);
    command("i2c unit w 2 7", 0);
    command("i2c unit save", 0);
    assert(!reads && writes == 3U);
    restart(argv[1]);
    assert(runtime.config_store.devices[0].whitelist_len == 1U && seed(2) == 7);
    assert(bvstk_i2c_cache_write(&runtime.cache, 0, 2, 9) == BVSTK_OK);
    bvstk_neutrino_i2c_runtime_lock(&runtime);
    assert(bvstk_neutrino_i2c_runtime_save_policy_locked(&runtime, 0));
    bvstk_neutrino_i2c_runtime_unlock(&runtime);
    restart(argv[1]);
    assert(seed(2) == 7); /* DCP2 policy SAVE preserves the configured seeds. */
    assert(bvstk_i2c_cache_write(&runtime.cache, 0, 2, 9) == BVSTK_OK);
    command("i2c unit save", 0);
    restart(argv[1]);
    assert(seed(2) == 9 && !reads && writes == 3U);

    n = contents(path, before, sizeof(before));
    command("i2c unit policy whitelist add 3 8", 0);
    assert(strlen(path) < sizeof(runtime.config_store.primary_dir));
    strcpy(runtime.config_store.primary_dir, path); /* A regular file cannot hold a profile. */
    command("i2c unit save", 1);
    assert(runtime.config_store.devices[0].whitelist_len == 2U);
    unchanged(path, before, n);
    restart(argv[1]);
    assert(runtime.config_store.devices[0].whitelist_len == 1U && seed(2) == 9);
    assert(!reads && writes == 3U);
    assert(!pthread_mutex_destroy(&runtime.state_mutex));
    puts("Neutrino CFG-RAM/SAVE/SEED: CLI, legacy DCP2, real files, restart and failed save passed");
    return 0;
}
