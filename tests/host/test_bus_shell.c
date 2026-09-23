/* Exercise the actual FreeRTOS command handlers with portable services and
 * in-memory bus/storage adapters. No UART, physical registers or flash writes. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "apps/freertos/console/i2c_shell.h"
#include "apps/freertos/console/mdio_shell.h"
#include "apps/freertos/console/stream_shell.h"
#include "apps/freertos/config/config_store.h"
#include "apps/freertos/runtime/bvstk_runtime.h"
#include "shared/cli/bvstk_i2c_completion.h"

static bvstk_i2c_master_service_t i2c;
static bvstk_i2c_devices_t devices;
static bvstk_i2c_cache_t cache;
static bvstk_i2c_policy_t policy;
static bvstk_mdio_service_t mdio;
static i2c_device_config_t i2c_config, i2c_saved;
static mdio_phy_config_t mdio_saved;
static unsigned i2c_saves, mdio_saves, writes;
static int ready = 1, save_ok = 1;
static bvstk_status_t io_status = BVSTK_OK;
static uint16_t registers[32];
static char output[16384];
static size_t output_length;

int console_stream_write(int fd, const void *data, size_t size)
{
    (void)fd;
    assert(output_length + size < sizeof(output));
    memcpy(output + output_length, data, size);
    output_length += size;
    output[output_length] = '\0';
    return (int)size;
}
void write_str(int fd, const char *text)
{
    (void)console_stream_write(fd, text, strlen(text));
}
bool dcp2_stream_shell_handle(uint8_t service, char **save, int fd)
{
    (void)service; (void)save; (void)fd;
    return true;
}
uint64_t bvstk_platform_now_ms(void) { return 1000U; }
void bvstk_platform_sleep_ms(uint32_t milliseconds) { (void)milliseconds; }
int bvstk_mmio_region_open(bvstk_mmio_region_t *r, uintptr_t base, size_t size)
{
    (void)r; (void)base; (void)size;
    assert(!"Unexpected physical MMIO access");
    return -1;
}
void bvstk_mmio_region_close(bvstk_mmio_region_t *r) { (void)r; }
int bvstk_mmio_read32(const bvstk_mmio_region_t *r, size_t offset, uint32_t *value)
{
    (void)r; (void)offset; (void)value;
    assert(!"Unexpected physical MMIO read");
    return -1;
}
int bvstk_mmio_write32(const bvstk_mmio_region_t *r, size_t offset, uint32_t value)
{
    (void)r; (void)offset; (void)value;
    assert(!"Unexpected physical MMIO write");
    return -1;
}

bvstk_i2c_master_service_t *bvstk_runtime_i2c_master_service(void) { return ready ? &i2c : NULL; }
bvstk_mdio_service_t *bvstk_runtime_mdio_service(void) { return ready ? &mdio : NULL; }
const i2c_device_config_t *config_store_find_i2c_device_by_name(const char *name)
{
    return strcasecmp(name, i2c_config.name) == 0 ? &i2c_config : NULL;
}
const i2c_device_config_t *config_store_find_i2c_device_by_addr(uint8_t address)
{
    return address == i2c_config.addr_7b ? &i2c_config : NULL;
}
int config_store_set_i2c_device(const i2c_device_config_t *config)
{
    i2c_config = *config;
    return 1;
}
int bvstk_runtime_i2c_apply_config(const i2c_device_config_t *config)
{
    return bvstk_i2c_master_service_set_config(&i2c, 0, config) == BVSTK_OK;
}
int bvstk_runtime_i2c_sync_device(size_t id)
{
    i2c_device_config_t next;
    assert(bvstk_i2c_master_service_get_config(&i2c, id, &next) == BVSTK_OK);
    strcpy(next.file_name, i2c_config.file_name);
    next.profile = i2c_config.profile;
    for (unsigned r = 0; r < next.reg_count; ++r) {
        uint8_t value;
        if (bvstk_i2c_cache_read(&cache, id, (uint8_t)r, &value) == BVSTK_OK) {
            next.settings[next.settings_len++] = (i2c_rule_entry_t){(uint8_t)r, value};
        }
    }
    i2c_config = next;
    return 1;
}
int config_store_save_i2c_device(const i2c_device_config_t *config)
{
    ++i2c_saves;
    if (save_ok) i2c_saved = *config;
    return save_ok;
}
int config_store_save_mdio_device(const mdio_phy_config_t *config)
{
    ++mdio_saves;
    if (save_ok) mdio_saved = *config;
    return save_ok;
}

static bvstk_status_t i2c_read(void *ctx, uint8_t address, uint8_t reg, uint8_t *value, uint32_t timeout)
{
    (void)ctx; (void)address; (void)timeout;
    *value = (uint8_t)registers[reg];
    return io_status;
}
static bvstk_status_t i2c_write(void *ctx, uint8_t address, uint8_t reg, uint8_t value, uint32_t timeout)
{
    (void)ctx; (void)address; (void)timeout;
    ++writes;
    if (io_status == BVSTK_OK) registers[reg] = value;
    return io_status;
}
static bvstk_status_t mdio_read(void *ctx, uint8_t phy, uint8_t reg, uint16_t *value, uint32_t timeout)
{
    (void)ctx; (void)phy; (void)timeout;
    *value = registers[reg];
    return io_status;
}
static bvstk_status_t mdio_write(void *ctx, uint8_t phy, uint8_t reg, uint16_t value,
                                bvstk_event_source_t source, uint32_t timeout)
{
    (void)ctx; (void)phy; (void)source; (void)timeout;
    ++writes;
    if (io_status == BVSTK_OK) registers[reg] = value;
    return io_status;
}

static void init_services(void)
{
    bvstk_i2c_master_io_t i2c_io = {.read_reg = i2c_read, .write_reg = i2c_write};
    bvstk_mdio_bus_ops_t mdio_io = {.read = mdio_read, .write = mdio_write};
    assert(bvstk_i2c_devices_init_from_config(&devices, &i2c_config, 1) == BVSTK_OK);
    assert(bvstk_i2c_cache_init(&cache, 1) == BVSTK_OK);
    assert(bvstk_i2c_policy_init(&policy, &i2c_config, 1) == BVSTK_OK);
    assert(bvstk_i2c_master_service_init(&i2c, NULL, &i2c_io, &devices, &cache, &policy, NULL) == BVSTK_OK);
    assert(bvstk_mdio_service_init(&mdio, NULL, &mdio_io, &mdio_saved, 1, NULL) == BVSTK_OK);
}
static void reset_fixture(void)
{
    i2c_config = (i2c_device_config_t){
        .name = "dev", .file_name = "dev.json", .addr_7b = 0x36, .reg_count = 16,
        .max_value_code = 64, .policy = I2C_POLICY_WHITELIST,
        .whitelist = {{2, 3}}, .whitelist_len = 1
    };
    mdio_saved = (mdio_phy_config_t){
        .name = "dev", .file_name = "dev.json", .phy_addr = 1, .reg_count = 16,
        .policy = MDIO_POLICY_WHITELIST, .whitelist = {{2, 3, 65535}}, .whitelist_len = 1
    };
    i2c_saved = i2c_config;
    i2c_saves = mdio_saves = writes = 0;
    ready = save_ok = 1;
    io_status = BVSTK_OK;
    memset(registers, 0, sizeof(registers));
    registers[2] = 3;
    init_services();
}
static void run(const char *command)
{
    char line[512], *save, *token;
    assert(strlen(command) < sizeof(line));
    strcpy(line, command);
    output_length = 0;
    output[0] = '\0';
    token = strtok_r(line, " \t", &save);
    assert(i2c_handle(token, &save, -1) || mdio_handle(token, &save, -1));
}
static void expect(const char *bus, const char *args, const char *expected)
{
    char command[512];
    snprintf(command, sizeof(command), "%s dev %s", bus, args);
    run(command);
    if (strcmp(output, expected) != 0) {
        fprintf(stderr, "%s\nExpected: %s\nActual: %s\n", command, expected, output);
        assert(!"Unexpected shell output");
    }
}
static void error(const char *bus, const char *args, const char *status)
{
    char expected[100];
    snprintf(expected, sizeof(expected), "ERR %s %s\r\n", bus, status);
    expect(bus, args, expected);
}

static void policy_tests(const char *bus)
{
    char rules[256], both[320];
    snprintf(rules, sizeof(rules), "whitelist (1):\r\n  reg=0x02 val=%s\r\n",
             strcmp(bus, "i2c") == 0 ? "0x03" : "0x0003 mask=0xFFFF");
    snprintf(both, sizeof(both), "policy=whitelist\r\n%sblacklist (0):\r\n", rules);
    expect(bus, "policy", "policy=whitelist\r\n");
    expect(bus, "policy show", "policy=whitelist\r\n");
    expect(bus, "PoLiCy ShOw", "policy=whitelist\r\n");
    expect(bus, "policy show rules", both);
    expect(bus, "policy show whitelist", rules);
    expect(bus, "policy whitelist", rules);
    expect(bus, "policy show blacklist", "blacklist (0):\r\n");
    expect(bus, "policy blacklist", "blacklist (0):\r\n");
    const char *invalid[] = {
        "policy show invalid", "policy show rules extra", "policy show whitelist extra",
        "policy set", "policy set invalid", "policy set blacklist extra",
        "policy whitelist add", "policy whitelist add 2", "policy whitelist clear extra",
        "policy whitelist invalid 2 3", "policy whitelist add 2 3 extra extra",
        "save extra", "r 2 extra", "w 2 3 extra", "info extra"
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) error(bus, invalid[i], "malformed");
    error(bus, "r 200", "range");
    error(bus, "r bad", "malformed");
    error(bus, "r -1", "malformed");
    error(bus, "w 2 4", "denied");
    error(bus, "w 2 99999999999999999999999999999", "range");
    expect(bus, "policy", "policy=whitelist\r\n");
    assert(writes == 0 && i2c_saves == 0 && mdio_saves == 0);
    expect(bus, "r 2", strcmp(bus, "i2c") == 0 ?
           "OK reg=0x02 val=0x03 dec=3\r\n" : "OK reg=0x02 val=0x0003 dec=3\r\n");
    expect(bus, "w 2 3", "OK\r\n");
    assert(writes == 1 && registers[2] == 3 && i2c_saves == 0 && mdio_saves == 0);
    io_status = BVSTK_ERR_TIMEOUT;
    error(bus, "r 2", "timeout");
    error(bus, "w 2 3", "timeout");
    io_status = BVSTK_OK;

    expect(bus, "policy whitelist add 4 5", "OK (RAM; use save to persist)\r\n");
    expect(bus, "policy whitelist del 4 5", "OK (RAM; use save to persist)\r\n");
    error(bus, "policy whitelist del 4 5", "not-found");
    expect(bus, "policy set blacklist", "OK (RAM; use save to persist)\r\n");
    expect(bus, "policy", "policy=blacklist\r\n");
    assert(i2c_saves == 0 && mdio_saves == 0);
    /* Recreate services from the saved image: unsaved edits must disappear. */
    i2c_config = i2c_saved;
    init_services();
    expect(bus, "policy", "policy=whitelist\r\n");
    expect(bus, "policy set blacklist", "OK (RAM; use save to persist)\r\n");
    save_ok = 0;
    error(bus, "save", "io");
    expect(bus, "policy", "policy=blacklist\r\n");
    assert(i2c_saved.policy == I2C_POLICY_WHITELIST && mdio_saved.policy == MDIO_POLICY_WHITELIST);
    save_ok = 1;
    char saved_line[100];
    snprintf(saved_line, sizeof(saved_line), "OK saved flash:/config/%s/dev.json\r\n", bus);
    expect(bus, "save", saved_line);
    assert((strcmp(bus, "i2c") == 0 ? i2c_saves : mdio_saves) == 2);
    i2c_config = i2c_saved;
    init_services();
    expect(bus, "policy", "policy=blacklist\r\n");
    ready = 0;
    error(bus, "policy", "not-ready");
    ready = 1;
}

static void completion_tests(void)
{
    bvstk_mdio_completion_device_t m = {.name = "dev", .phy_addr = 1};
    bvstk_i2c_completion_result_t ir;
    bvstk_mdio_completion_result_t mr;
    const char *tails[] = {"policy ", "policy show ", "policy set ", "policy whitelist ", "policy blacklist ", "sa"};
    for (size_t n = 0; n < sizeof(tails) / sizeof(tails[0]); ++n) {
        char line[100];
        snprintf(line, sizeof(line), "i2c dev %s", tails[n]);
        assert(i2c_complete_line(line, strlen(line), strlen(line), &ir));
        snprintf(line, sizeof(line), "mdio dev %s", tails[n]);
        assert(bvstk_mdio_complete_line(line, strlen(line), strlen(line), &m, 1, &mr));
        assert(ir.match_count == mr.match_count);
        for (size_t j = 0; j < ir.match_count; ++j) assert(strcmp(ir.candidates[j], mr.candidates[j]) == 0);
    }
    const char *no_match[] = {"policy whitelist add 2 ", "policy set whitelist ", "policy show rules ", "save "};
    for (size_t n = 0; n < sizeof(no_match) / sizeof(no_match[0]); ++n) {
        char line[100];
        snprintf(line, sizeof(line), "i2c dev %s", no_match[n]);
        assert(!i2c_complete_line(line, strlen(line), strlen(line), &ir));
        snprintf(line, sizeof(line), "mdio dev %s", no_match[n]);
        assert(!bvstk_mdio_complete_line(line, strlen(line), strlen(line), &m, 1, &mr));
    }
}

int main(void)
{
    reset_fixture();
    policy_tests("i2c");
    reset_fixture();
    policy_tests("mdio");
    reset_fixture();
    expect("i2c", "addr 0x37", "OK (RAM; use save to persist)\r\n");
    assert(i2c_config.addr_7b == 0x37 && i2c_saved.addr_7b == 0x36 && i2c_saves == 0);
    run("i2c @0x37 policy");
    assert(strcmp(output, "policy=whitelist\r\n") == 0);
    expect("i2c", "save", "OK saved flash:/config/i2c/dev.json\r\n");
    assert(i2c_saved.addr_7b == 0x37);
    completion_tests();
    puts("I2C/MDIO shell output, policy grammar, errors, explicit save and completion tests passed");
    return 0;
}
