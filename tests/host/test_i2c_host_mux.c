/* Regression: the host-facing PCA9545A view must follow what the external
 * master selected, not the downstream switch state seen by PS-internal I/O.
 *
 * Field case (AX7020, 2026-11-10): the host's Linux pca954x driver selected
 * channel 1 (SFP) once, but that forwarded write never reached the real
 * switch.  Background presence checks then re-read the real switch (still
 * channel 0), so every later host read of 0x51 resolved to the PCF8563 on
 * channel 0 instead of the SFP diagnostics page.  The pointer 0x6E is out of
 * range for the RTC, the slave service gave no reply window and the PL core
 * NACKed after its 50 ms stretch timeout.  Linux does not re-select a channel
 * it believes is already selected, so the state never recovered. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "services/i2c/bvstk_i2c_slave_service.h"
#include "shared/config/bvstk_device_profile.h"
#include "shared/config/bvstk_i2c_config_codec.h"

static i2c_device_config_t configs[8];
static bvstk_i2c_devices_t devices;
static bvstk_i2c_cache_t cache;
static bvstk_i2c_policy_t policy;
static bvstk_i2c_master_service_t master;
static bvstk_i2c_slave_service_t slave;
static int held;
static uint8_t real_switch = 0x01;   /* downstream PCA9545A control register */
static int switch_writes_fail;       /* downstream switch NACKs writes */
static uint8_t last_sfp_pointer = 0xFF, last_rtc_pointer = 0xFF;

uint64_t bvstk_platform_now_ms(void) { return 0; }
void bvstk_platform_sleep_ms(uint32_t ms) { (void)ms; }
int bvstk_mmio_region_open(bvstk_mmio_region_t *r, uintptr_t b, size_t s) { (void)r; (void)b; (void)s; return -1; }
void bvstk_mmio_region_close(bvstk_mmio_region_t *r) { (void)r; }
int bvstk_mmio_read32(const bvstk_mmio_region_t *r, size_t o, uint32_t *v) { (void)r; (void)o; (void)v; return -1; }
int bvstk_mmio_write32(const bvstk_mmio_region_t *r, size_t o, uint32_t v) { (void)r; (void)o; (void)v; return -1; }

static bvstk_status_t lock_bus(void *c, uint32_t ms) { (void)c; (void)ms; assert(!held); held = 1; return BVSTK_OK; }
static void unlock_bus(void *c) { (void)c; assert(held); held = 0; }

/* Downstream model: PCA9545A at 0x70; SFP (0x50/0x51) on channel 1;
 * PCF8563 (0x51) and EEPROM (0x57) on channel 0; PCA9555 (0x20) on channel 3. */
static bvstk_status_t transfer(void *c, uint8_t a, const uint8_t *tx, size_t nt, uint8_t *rx, size_t nr, uint32_t ms)
{
    (void)c; (void)ms;
    assert(held);
    if (a == 0x70) {
        if (nt && switch_writes_fail) return BVSTK_ERR_IO;
        if (nt) real_switch = tx[0] & 0x0F;
        if (nr) rx[0] = (uint8_t)(0xF0 | real_switch);
        return BVSTK_OK;
    }
    if ((a == 0x50 || a == 0x51) && (real_switch & 0x02)) {
        if (nt) last_sfp_pointer = tx[0];
        for (size_t i = 0; i < nr; ++i) rx[i] = (uint8_t)(0xA0 + i);
        return BVSTK_OK;
    }
    if ((a == 0x51 || a == 0x57) && (real_switch & 0x01)) {
        if (nt && a == 0x51) last_rtc_pointer = tx[0];
        for (size_t i = 0; i < nr; ++i) rx[i] = 0x11;
        return BVSTK_OK;
    }
    if (a == 0x20 && (real_switch & 0x08)) {
        for (size_t i = 0; i < nr; ++i) rx[i] = 0x22;
        return BVSTK_OK;
    }
    return BVSTK_ERR_IO;
}
static void on_event(void *ctx, const bvstk_event_t *e) { (void)ctx; (void)e; }

static void load(const char *root, const char *rel, i2c_device_config_t *cfg)
{
    static char json[BVSTK_PROFILE_JSON_MAX + 1];
    char path[512];
    snprintf(path, sizeof(path), "%s/i2c/%s", root, rel);
    FILE *f = fopen(path, "rb");
    assert(f);
    size_t n = fread(json, 1, sizeof(json) - 1, f);
    json[n] = 0;
    fclose(f);
    assert(bvstk_i2c_config_parse_json(json, cfg) == BVSTK_OK);
    strncpy(cfg->file_name, rel, sizeof(cfg->file_name) - 1);
}

static size_t id_of(const char *name)
{
    size_t id;
    assert(bvstk_i2c_devices_find_by_name(&devices, name, &id) == BVSTK_OK);
    return id;
}

/* The FreeRTOS slave adapter's write path: select by address, then a frame. */
static bvstk_status_t host_write(uint8_t addr, const uint8_t *data, size_t n)
{
    size_t ignored = 0;
    uint8_t window[8];
    bvstk_status_t st = bvstk_i2c_slave_service_select_address(&slave, addr);
    if (st != BVSTK_OK) return st;
    return bvstk_i2c_slave_service_handle_frame(&slave, data, n, 0, window, sizeof(window), &ignored, 20);
}

/* The read path: RD_REQUEST -> select by address -> reply window. */
static bvstk_status_t host_read(uint8_t addr, uint8_t *value)
{
    size_t size = 0;
    bvstk_status_t st = bvstk_i2c_slave_service_select_address(&slave, addr);
    if (st != BVSTK_OK) return st;
    st = bvstk_i2c_slave_service_handle_frame(&slave, NULL, 0, 1, value, 1, &size, 20);
    if (st == BVSTK_OK && size != 1) return BVSTK_ERR_NOT_READY;
    return st;
}

static void init(const char *root)
{
    const char *files[] = {"axp15060/axp15060.json", "pca9545a/pca9545a.json", "pca9545a/p24c02a/p24c02a.json",
                           "pca9545a/pca9555/pca9555.json", "pca9545a/pcf8563/pcf8563.json", "pca9545a/sfp/sfp.json"};
    size_t count = sizeof(files) / sizeof(files[0]);
    for (size_t i = 0; i < count; ++i) load(root, files[i], &configs[i]);
    const bvstk_i2c_master_io_t io = {.transfer = transfer, .lock = lock_bus, .unlock = unlock_bus};
    const bvstk_event_sink_t sink = {.publish = on_event};
    assert(bvstk_i2c_devices_init_from_config(&devices, configs, count) == BVSTK_OK);
    assert(bvstk_i2c_cache_init(&cache, count) == BVSTK_OK);
    assert(bvstk_i2c_policy_init(&policy, configs, count) == BVSTK_OK);
    assert(bvstk_i2c_master_service_init(&master, NULL, &io, &devices, &cache, &policy, &sink) == BVSTK_OK);
    assert(bvstk_i2c_slave_service_init(&slave, &devices, &cache, &master, 0) == BVSTK_OK);
}

int main(int argc, char **argv)
{
    uint8_t value = 0;
    const uint8_t select_sfp = 0x02, select_rtc = 0x01, status_pointer = 0x6E, seconds = 0x02;
    init(argc > 1 ? argv[1] : "configs");
    size_t sfp = id_of("sfp"), rtc = id_of("pcf8563");

    /* 1. Normal case: the host selects channel 1 and reads SFP A2h:0x6E. */
    assert(host_write(0x70, &select_sfp, 1) == BVSTK_OK && real_switch == 0x02);
    assert(host_write(0x51, &status_pointer, 1) == BVSTK_OK && slave.target_device == sfp);
    assert(host_read(0x51, &value) == BVSTK_OK && slave.target_device == sfp && last_sfp_pointer == 0x6E);

    /* 2. Field case: the host goes back to channel 0 and then selects
     *    channel 1, but the forwarded write does not reach the real switch. */
    assert(host_write(0x70, &select_rtc, 1) == BVSTK_OK && real_switch == 0x01);
    switch_writes_fail = 1;
    (void)host_write(0x70, &select_sfp, 1);          /* ACKed on the host bus by the PL core */
    switch_writes_fail = 0;
    assert(real_switch == 0x01);
    /* A background presence check re-reads the real switch (still channel 0). */
    assert(bvstk_i2c_master_service_probe(&master, rtc, 0, 20) == BVSTK_OK);
    /* The host believes channel 1 is selected and does not write 0x70 again. */
    last_sfp_pointer = last_rtc_pointer = 0xFF;
    assert(host_write(0x51, &status_pointer, 1) == BVSTK_OK);
    assert(slave.target_device == sfp);
    assert(host_read(0x51, &value) == BVSTK_OK);
    assert(slave.target_device == sfp && last_sfp_pointer == 0x6E && last_rtc_pointer == 0xFF);
    /* PS-internal routing restored the downstream switch afterwards. */
    assert(real_switch == 0x01);

    /* 3. The host re-selects channel 0: 0x51 is the RTC again. */
    assert(host_write(0x70, &select_rtc, 1) == BVSTK_OK);
    assert(host_write(0x51, &seconds, 1) == BVSTK_OK && slave.target_device == rtc);
    assert(host_read(0x51, &value) == BVSTK_OK && slave.target_device == rtc && value == 0x11);

    /* 4. A policy-denied selection is not a selection: the host view keeps channel 0. */
    const uint8_t forbidden = 0x0F;   /* several channels at once */
    if (host_write(0x70, &forbidden, 1) == BVSTK_ERR_DENIED) {
        assert(host_write(0x51, &seconds, 1) == BVSTK_OK && slave.target_device == rtc);
    }

    printf("I2C host mux view: diverged downstream switch, re-selection and denied selection passed\n");
    return 0;
}
