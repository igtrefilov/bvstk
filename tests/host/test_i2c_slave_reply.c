/* Exercise the real FreeRTOS adapter, slave service and PL driver. Downstream
 * I/O can finish after the host leaves; failed/unpublished replies must not
 * consume the register that the next current-address read should return. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ports/freertos-xilinx/os/i2c/bvstk_i2c_slave_freertos.c"
#include "shared/config/bvstk_i2c_config_codec.h"

static i2c_device_config_t configs[2];
static bvstk_i2c_devices_t devices;
static bvstk_i2c_cache_t cache;
static bvstk_i2c_policy_t policy;
static bvstk_i2c_master_service_t master;
static bvstk_i2c_slave_service_t service;
static bvstk_i2c_slave_hw_t hardware;
static bvstk_i2c_slave_freertos_t adapter;
static uint32_t regs[32], bram[4096];
static unsigned held, backend_reads, accepted;
static int end_during_read, fail_backend, fail_window, fail_accept;
static slave_queue_item_t queued;

uint64_t bvstk_platform_now_ms(void) { return 0; }
void bvstk_platform_sleep_ms(uint32_t ms) { (void)ms; }
int bvstk_mmio_region_open(bvstk_mmio_region_t *r, uintptr_t b, size_t s)
{ (void)r; (void)b; (void)s; return -1; }
void bvstk_mmio_region_close(bvstk_mmio_region_t *r) { (void)r; }
int bvstk_mmio_read32(const bvstk_mmio_region_t *r, size_t off, uint32_t *value)
{
    assert(off + sizeof(*value) <= r->size);
    memcpy(value, (const void *)(r->mapped_base + off), sizeof(*value));
    return 0;
}
int bvstk_mmio_write32(const bvstk_mmio_region_t *r, size_t off, uint32_t value)
{
    assert(off + sizeof(value) <= r->size);
    if (r == &hardware.bram && fail_window) return -1;
    if (r == &hardware.slave && off == BVSTK_I2C_SLV_CSR_OFFSET) {
        assert(value == BVSTK_I2C_SLV_CSR_RD_VALID_BIT);
        if (fail_accept) return -1;
        ++accepted;
    }
    if (r == &hardware.slave && off == BVSTK_I2C_SLV_IRQ_OFFSET)
        value = 0; /* IRQ acknowledgement clears the modeled latch. */
    memcpy((void *)(r->mapped_base + off), &value, sizeof(value));
    return 0;
}
int xQueueSendFromISR(QueueHandle_t q, const void *item, int *woken)
{
    assert(q == &queued);
    queued = *(const slave_queue_item_t *)item;
    *woken = 0;
    return pdTRUE;
}
static bvstk_status_t lock_bus(void *c, uint32_t ms)
{ (void)c; (void)ms; assert(!held); held = 1; return BVSTK_OK; }
static void unlock_bus(void *c) { (void)c; assert(held); held = 0; }
static bvstk_status_t transfer(void *c, uint8_t address, const uint8_t *tx,
    size_t nt, uint8_t *rx, size_t nr, uint32_t ms)
{
    (void)c; (void)ms;
    assert(held && nt == 1 && nr == 1);
    assert(address == 0x57 || address == 0x50 || address == 0x51);
    ++backend_reads;
    if (fail_backend) return BVSTK_ERR_IO;
    rx[0] = (uint8_t)(tx[0] + (address == 0x57 ? 0x10 : address == 0x50 ? 0x30 : 0x70));
    if (end_during_read == 1) regs[BVSTK_I2C_SLV_CSR_OFFSET / 4] = 0;
    if (end_during_read == 2) regs[BVSTK_I2C_SLV_REQ_OFFSET / 4] ^= 1U;
    return BVSTK_OK;
}
static void event(void *ctx, const bvstk_event_t *e) { (void)ctx; (void)e; }

static void load(const char *root, const char *name, i2c_device_config_t *config)
{
    static char json[BVSTK_PROFILE_JSON_MAX + 1];
    char path[512];
    snprintf(path, sizeof(path), "%s/i2c/pca9545a/%s/%s.json", root, name, name);
    FILE *f = fopen(path, "rb");
    assert(f);
    size_t n = fread(json, 1, sizeof(json) - 1, f);
    json[n] = 0;
    fclose(f);
    assert(bvstk_i2c_config_parse_json(json, config) == BVSTK_OK);
    /* Routing is tested separately; put the real endpoint profiles on one bus. */
    config->endpoint.parent[0] = 0;
    config->endpoint.channel = 0;
}
static void init(const char *root)
{
    load(root, "p24c02a", &configs[0]);
    load(root, "sfp", &configs[1]);
    const bvstk_i2c_master_io_t io = {.transfer = transfer, .lock = lock_bus, .unlock = unlock_bus};
    const bvstk_event_sink_t sink = {.publish = event};
    assert(bvstk_i2c_devices_init_from_config(&devices, configs, 2) == BVSTK_OK);
    assert(bvstk_i2c_cache_init(&cache, 2) == BVSTK_OK);
    assert(bvstk_i2c_policy_init(&policy, configs, 2) == BVSTK_OK);
    assert(bvstk_i2c_master_service_init(&master, NULL, &io, &devices, &cache, &policy, &sink) == BVSTK_OK);
    assert(bvstk_i2c_slave_service_init(&service, &devices, &cache, &master, 0) == BVSTK_OK);
    hardware.slave.mapped_base = (volatile uint8_t *)regs;
    hardware.slave.size = sizeof(regs);
    hardware.bram.mapped_base = (volatile uint8_t *)bram;
    hardware.bram.size = sizeof(bram);
    hardware.initialized = 1;
    adapter.hardware = &hardware;
    adapter.service = &service;
    adapter.event_queue = &queued;
}
static void pointer(uint8_t address, uint8_t reg)
{
    uint8_t window[64];
    slave_queue_item_t item = {.frame_status = BVSTK_OK, .frame_size = 1, .frame_addr = address};
    item.frame[0] = reg;
    assert(process_write_event(&adapter, &item, window, sizeof(window)) == BVSTK_OK);
}
static bvstk_status_t read_reply(uint8_t address)
{
    uint8_t window[64];
    bvstk_i2c_slave_irq_event_t request = {.request_addr = address, .request_header = address};
    regs[BVSTK_I2C_SLV_CSR_OFFSET / 4] = BVSTK_I2C_SLV_STATUS_BUSY_BIT;
    regs[BVSTK_I2C_SLV_REQ_OFFSET / 4] = address;
    accepted = 0;
    return process_read_event(&adapter, &request, window, sizeof(window));
}
static void expect_reply(uint8_t address, uint8_t expected)
{
    assert(read_reply(address) == BVSTK_OK && accepted == 1);
    uint8_t actual = ((uint8_t *)bram)[BVSTK_I2C_BRAM_SLAVE_RD_OFFSET + 4];
    if (actual != expected) {
        fprintf(stderr, "reply at 0x%02x: got 0x%02x, expected 0x%02x\n", address, actual, expected);
        assert(actual == expected);
    }
}

int main(int argc, char **argv)
{
    init(argc > 1 ? argv[1] : "configs");
    pointer(0x57, 0);
    expect_reply(0x57, 0x10);
    end_during_read = 1;
    assert(read_reply(0x57) == BVSTK_OK && accepted == 0);
    end_during_read = 0;
    expect_reply(0x57, 0x11); /* cancelled prefetch did not consume register 1 */

    end_during_read = 2;
    assert(read_reply(0x57) == BVSTK_OK && accepted == 0);
    end_during_read = 0;
    expect_reply(0x57, 0x12);

    fail_window = 1;
    assert(read_reply(0x57) == BVSTK_ERR_IO && accepted == 0);
    fail_window = 0;
    expect_reply(0x57, 0x13);
    fail_accept = 1;
    assert(read_reply(0x57) == BVSTK_ERR_IO && accepted == 0);
    fail_accept = 0;
    expect_reply(0x57, 0x14);
    fail_backend = 1;
    assert(read_reply(0x57) == BVSTK_ERR_IO && accepted == 0);
    fail_backend = 0;
    expect_reply(0x57, 0x15);

    /* An already-stale event must not even start downstream I/O. */
    unsigned reads = backend_reads;
    uint8_t window[64];
    bvstk_i2c_slave_irq_event_t request = {.request_addr = 0x57, .request_header = 0x57};
    regs[BVSTK_I2C_SLV_CSR_OFFSET / 4] = 0;
    assert(process_read_event(&adapter, &request, window, sizeof(window)) == BVSTK_OK);
    assert(backend_reads == reads);

    /* A cancelled last-register read must preserve the wrap boundary. */
    pointer(0x57, 0xff);
    end_during_read = 1;
    assert(read_reply(0x57) == BVSTK_OK && accepted == 0);
    end_during_read = 0;
    expect_reply(0x57, 0x0f);
    expect_reply(0x57, 0x10);

    /* Independent pointers for the two SFP address spaces. */
    pointer(0x50, 0);
    pointer(0x51, 0x6e);
    end_during_read = 1;
    assert(read_reply(0x51) == BVSTK_OK && accepted == 0);
    end_during_read = 0;
    expect_reply(0x50, 0x30);
    expect_reply(0x51, 0xde);
    expect_reply(0x50, 0x31);

    /* Queued pointer bytes survive reuse of the single BRAM write window. */
    regs[BVSTK_I2C_SLV_IRQ_OFFSET / 4] = BVSTK_I2C_SLV_IRQ_DATA_VALID | BVSTK_I2C_SLV_IRQ_FINAL_PACKET;
    bram[BVSTK_I2C_BRAM_SLAVE_WR_OFFSET / 4] = (1U << BVSTK_I2C_SLV_HEADER_BYTES_SHIFT) | 0x57;
    bram[BVSTK_I2C_BRAM_SLAVE_WR_OFFSET / 4 + 1] = 0x2a;
    slave_isr(&adapter);
    bram[BVSTK_I2C_BRAM_SLAVE_WR_OFFSET / 4 + 1] = 0x55;
    assert(process_write_event(&adapter, &queued, window, sizeof(window)) == BVSTK_OK);
    expect_reply(0x57, 0x3a);

    /* Preparation can be abandoned; only committing one of these equivalent
     * replies advances the cursor, and a committed token cannot be reused. */
    bvstk_i2c_slave_read_reply_t abandoned, reply;
    size_t size;
    assert(bvstk_i2c_slave_service_prepare_read(&service, window, sizeof(window),
        &size, &abandoned, 20) == BVSTK_OK && size == 1 && window[0] == 0x3b);
    assert(bvstk_i2c_slave_service_prepare_read(&service, window, sizeof(window),
        &size, &reply, 20) == BVSTK_OK && size == 1 && window[0] == 0x3b);
    assert(bvstk_i2c_slave_service_commit_read(&service, &reply) == BVSTK_OK);
    assert(bvstk_i2c_slave_service_commit_read(&service, &reply) == BVSTK_ERR_MALFORMED);
    assert(bvstk_i2c_slave_service_commit_read(&service, &abandoned) == BVSTK_ERR_BUSY);
    expect_reply(0x57, 0x3c);

    /* A pointer write invalidates the older prepared movement. */
    assert(bvstk_i2c_slave_service_prepare_read(&service, window, sizeof(window),
        &size, &reply, 20) == BVSTK_OK && size == 1);
    pointer(0x57, 0x42);
    assert(bvstk_i2c_slave_service_commit_read(&service, &reply) == BVSTK_ERR_BUSY);
    expect_reply(0x57, 0x52);

    /* A failing preparation must not leave a previous token usable. */
    assert(bvstk_i2c_slave_service_prepare_read(&service, window, sizeof(window),
        &size, &reply, 20) == BVSTK_OK && size == 1);
    fail_backend = 1;
    assert(bvstk_i2c_slave_service_prepare_read(&service, window, sizeof(window),
        &size, &reply, 20) == BVSTK_ERR_IO);
    assert(bvstk_i2c_slave_service_commit_read(&service, &reply) == BVSTK_ERR_MALFORMED);
    fail_backend = 0;
    expect_reply(0x57, 0x53);
    puts("I2C slave replies: cancelled/failed replies preserve pointers, wrap, spaces and ISR snapshot passed");
    return 0;
}
