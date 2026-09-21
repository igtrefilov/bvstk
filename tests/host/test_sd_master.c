/* Black-box contract emulator, NOT an RTL model. Hardware conformance must
 * still be measured on the board. Literal offsets/lengths below deliberately
 * cross-check the portable driver's packet and DMA register definitions. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "services/sd/bvstk_sd_service.h"

enum fault {
    NONE, NO_IRQ, STALE_REPLY, DMA_ERROR, SHORT_RX, OVERRUN, BAD_TAIL,
    BAD_CRC, WRONG_DATA, NO_COMPLETION, BAD_INIT, STALE_OCR, RESET_STUCK,
    RX_NOT_DONE, HALTED_TX, MMIO_ERROR, CARD_REJECT
};
static struct {
    uint32_t core[0x10000 / 4], dma[0x10000 / 4], bram[0x2000 / 4];
    uint8_t media[16][512];
    uint64_t ms;
    uint32_t irqs, resets, commands, writes, last_arg;
    unsigned locks, unlocks;
    bool enabled, locked, sdsc, alias, idle_reply;
    bool require_isolation, command_since_init;
    uint8_t read_descriptor_bytes;
    enum fault fault;
} bus;

static uint16_t reference_crc(const uint8_t *p, size_t n)
{
    uint32_t remainder = 0;
    size_t i;
    unsigned b;
    for (i = 0; i < n + 2; ++i)
        for (b = 0; b < 8; ++b) {
            remainder = (remainder << 1) | (i < n ? ((p[i] >> (7 - b)) & 1U) : 0U);
            if (remainder & 0x10000U) remainder ^= 0x11021U;
        }
    return (uint16_t)remainder;
}

static void bram_byte(uint32_t offset, uint8_t value)
{
    uint32_t shift = (offset % 4) * 8;
    bus.bram[offset / 4] = (bus.bram[offset / 4] & ~(0xffU << shift)) |
                          ((uint32_t)value << shift);
}

static void complete(void)
{
    uint32_t header = bus.bram[0x404 / 4], arg = bus.bram[0x400 / 4];
    uint32_t command = header & 0x3fU, mailbox, lba, i;
    uint8_t bytes[512];
    uint16_t crc;
    assert(bus.locked);
    assert(bus.dma[0x18 / 4] == 0x44000400);
    assert(bus.dma[0x28 / 4] == (command == 24 ? 522U :
        (command == 17 ? bus.read_descriptor_bytes : 8U)));
    if (command == 17 && bus.read_descriptor_bytes == 16) {
        assert(bus.bram[0x408 / 4] == arg);
        assert(bus.bram[0x40c / 4] == ((header & 0xffU) | 0xff00U));
    }
    assert((header & 0xc0) == 0x40);
    bus.last_arg = ((arg & 0xffU) << 24) | ((arg & 0xff00U) << 8) |
                   ((arg >> 8) & 0xff00U) | (arg >> 24);
    ++bus.commands;
    if (command == 24) ++bus.writes;
    if (bus.require_isolation && bus.command_since_init) return;
    bus.command_since_init = true;
    switch (command) {
    case 13: mailbox = 0x34; break;
    case 16: mailbox = 0x38; assert(bus.last_arg == 512); break;
    case 17: mailbox = 0x3c; break;
    case 24: mailbox = 0x44; break;
    default: assert(0); return;
    }
    if (bus.fault == NO_COMPLETION) return;
    if (bus.fault != STALE_REPLY) bus.bram[mailbox / 4] = 0;
    if (bus.idle_reply) bus.bram[mailbox / 4] = 0xff;
    if (bus.fault == CARD_REJECT) bus.bram[mailbox / 4] = 4;
    bus.dma[4 / 4] = bus.fault == DMA_ERROR ? 0x4010 : 0x1002;
    if (bus.fault == HALTED_TX) bus.dma[4 / 4] |= 1;
    if (bus.enabled && bus.fault != NO_IRQ) ++bus.irqs;
    lba = bus.sdsc ? bus.last_arg / 512U : bus.last_arg;
    lba = bus.alias ? 0 : lba % 16;
    if (command == 24) {
        for (i = 0; i < 512; ++i)
            bytes[i] = (uint8_t)(bus.bram[(0x408 + (i & ~3U)) / 4] >> ((3U - (i % 4)) * 8));
        assert((bus.bram[0x608 / 4] & 0xffffU) == reference_crc(bytes, 512));
        memcpy(bus.media[lba], bytes, 512);
    } else if (command == 17) {
        assert(bus.dma[0x48 / 4] == 0x44001000);
        assert(bus.dma[0x58 / 4] == 514);
        assert(bus.dma[0x30 / 4] == 1); /* RX was armed before TX length. */
        memcpy(bytes, bus.media[lba], 512);
        if (bus.fault == WRONG_DATA) bytes[41] ^= 1;
        crc = reference_crc(bytes, 512);
        if (bus.fault == BAD_CRC) crc ^= 1;
        for (i = 0; i < 512; ++i) bram_byte(0x1000 + i, bytes[i]);
        bram_byte(0x1200, (uint8_t)(crc >> 8));
        bram_byte(0x1201, (uint8_t)crc);
        bus.dma[0x34 / 4] = bus.fault == RX_NOT_DONE ? 2 : 0x1002;
        if (bus.fault == SHORT_RX) bus.dma[0x58 / 4] = 512;
        if (bus.fault == OVERRUN) bus.bram[0x1204 / 4] = 0;
        if (bus.fault == BAD_TAIL) bram_byte(0x1202, 0);
    }
}

int bvstk_mmio_region_open(bvstk_mmio_region_t *r, uintptr_t base, size_t size)
{
    r->physical_base = base;
    r->size = size;
    if (base == 0x43c40000) r->mapped_base = (volatile uint8_t *)bus.core;
    else if (base == 0x40400000) r->mapped_base = (volatile uint8_t *)bus.dma;
    else if (base == 0x44000000) r->mapped_base = (volatile uint8_t *)bus.bram;
    else return -1;
    return 0;
}

void bvstk_mmio_region_close(bvstk_mmio_region_t *r) { memset(r, 0, sizeof(*r)); }

int bvstk_mmio_read32(const bvstk_mmio_region_t *r, size_t off, uint32_t *value)
{
    assert(off + 4 <= r->size && off % 4 == 0);
    if (bus.fault == MMIO_ERROR) return -1;
    *value = *(volatile uint32_t *)(r->mapped_base + off);
    return 0;
}

int bvstk_mmio_write32(const bvstk_mmio_region_t *r, size_t off, uint32_t value)
{
    assert(off + 4 <= r->size && off % 4 == 0);
    if (bus.fault == MMIO_ERROR) return -1;
    if (r->physical_base == 0x40400000 && (off == 4 || off == 0x34)) {
        bus.dma[off / 4] &= ~value;
        return 0;
    }
    *(volatile uint32_t *)(r->mapped_base + off) = value;
    if (r->physical_base == 0x40400000 && off == 0 && value == 4) {
        memset(bus.dma, 0, sizeof(bus.dma));
        bus.dma[0] = bus.fault == RESET_STUCK ? 4 : 0;
        bus.dma[1] = bus.dma[0x34 / 4] = 1;
    }
    if (r->physical_base == 0x40400000 && off == 0x28) {
        uint8_t cmd = (uint8_t)(bus.bram[0x404 / 4] & 0x3fU);
        if (cmd == 17) assert(bus.dma[0x58 / 4] == 514);
    }
    if (r->physical_base == 0x43c40000) {
        if (off == 0 && value == 1) ++bus.resets;
        if (off == 0 && value == 14) complete();
        if (off == 0x14 && value == 0x193) {
            const uint32_t replies[] = {1, 0x1aa, 1, 0, 0, 0xc0ff8000, 0};
            assert(bus.locked);
            bus.command_since_init = false;
            if (bus.fault != NO_IRQ) ++bus.irqs;
            if (bus.fault != NO_COMPLETION) {
                memcpy(bus.bram, replies, sizeof(replies));
                if (bus.sdsc) bus.bram[5] &= ~UINT32_C(0x40000000);
                if (bus.fault == BAD_INIT) bus.bram[1] = 0x1ab;
                if (bus.fault == STALE_OCR) bus.bram[5] = 0xa5963cc3;
            }
        }
    }
    return 0;
}

uint64_t bvstk_platform_now_ms(void) { return bus.ms; }
void bvstk_platform_sleep_ms(uint32_t ms) { bus.ms += ms; }
static bvstk_status_t prepare(void *ctx)
{
    (void)ctx;
    bus.irqs = 0;
    bus.enabled = true;
    return BVSTK_OK;
}
static uint32_t count(void *ctx) { (void)ctx; return bus.irqs; }
static void disable(void *ctx) { (void)ctx; bus.enabled = false; }
static void barrier(void *ctx) { (void)ctx; }
static bvstk_status_t lock(void *ctx, uint32_t timeout)
{
    (void)ctx; (void)timeout;
    if (bus.locked) return BVSTK_ERR_BUSY;
    bus.locked = true;
    ++bus.locks;
    return BVSTK_OK;
}
static void unlock(void *ctx)
{
    (void)ctx;
    assert(bus.locked);
    bus.locked = false;
    ++bus.unlocks;
}

static bvstk_sd_master_t driver;
static bvstk_sd_service_t service;
static bvstk_sd_result_t result;

static void setup(bool sdsc)
{
    const bvstk_sd_master_config_t cfg = {0x43c40000, 0x40400000, 0x44000000,
        0x2000, 512, 512, {NULL, prepare, count, disable, barrier}, false, 8};
    const bvstk_mutex_t mutex = {NULL, lock, unlock};
    memset(&bus, 0, sizeof(bus));
    bus.sdsc = sdsc;
    bus.read_descriptor_bytes = 8;
    assert(bvstk_sd_master_open(&driver, &cfg, NULL) == BVSTK_OK);
    assert(bvstk_sd_service_attach(&service, &driver, &mutex, false) == BVSTK_OK);
    assert(bus.commands == 0 && bus.writes == 0 && bus.resets == 0);
}

static void initialize(void)
{
    uint32_t writes = bus.writes;
    assert(bvstk_sd_service_initialize(&service, 20, &result) == BVSTK_OK);
    assert(service.info.ready);
    assert(bus.writes == writes);
}

static void test_vectors(void)
{
    uint32_t words[2];
    const uint8_t cmd0[] = {0x40, 0, 0, 0, 0};
    const uint8_t cmd8[] = {0x48, 0, 0, 1, 0xaa};
    assert(bvstk_sd_master_crc7(cmd0, sizeof(cmd0)) == 0x95);
    assert(bvstk_sd_master_crc7(cmd8, sizeof(cmd8)) == 0x87);
    assert(bvstk_sd_master_crc16((const uint8_t *)"123456789", 9) == 0x31c3);
    bvstk_sd_master_encode(16, 512, words);
    assert(words[0] == 0x00020000 && (words[1] & 0xff) == 0x50);
    bvstk_sd_master_encode(24, 0x12345678, words);
    assert(words[0] == 0x78563412 && (words[1] & 0xff) == 0x58);
}

static void test_success(bool sdsc)
{
    uint8_t block[512], readback[512];
    unsigned locks;
    setup(sdsc);
    assert(bvstk_sd_service_read(&service, 9, block, &result) == BVSTK_ERR_NOT_READY);
    initialize();
    assert(bus.commands == (sdsc ? 1U : 0U));
    bvstk_sd_service_pattern(block, 9, 0x12345678);
    locks = bus.locks;
    assert(bvstk_sd_service_write_verify(&service, 9, block, &result) == BVSTK_OK);
    assert(bus.locks == locks + 1 && bus.locks == bus.unlocks);
    assert(result.write_hardware.command == 24 && result.hardware.command == 17);
    assert(bus.last_arg == (sdsc ? 4608U : 9U));
    assert(bvstk_sd_service_read(&service, 9, readback, &result) == BVSTK_OK);
    assert(memcmp(block, readback, 512) == 0);
    assert(bvstk_sd_service_card_status(&service, &result) == BVSTK_OK);
    assert(bvstk_sd_service_block_length(&service, &result) == BVSTK_OK);
    assert(bvstk_sd_service_test(&service, 4, 8, 0x01020304, &result) == BVSTK_OK);
    assert(bus.resets == 1); /* No hidden core reset/reinit between good I/O. */
    assert(bus.writes == 9);
    assert(bus.locks == bus.unlocks);
}

static void test_faults(void)
{
    const enum fault faults[] = {NO_IRQ, STALE_REPLY, DMA_ERROR, SHORT_RX,
        OVERRUN, BAD_TAIL, BAD_CRC, NO_COMPLETION, RX_NOT_DONE, HALTED_TX,
        MMIO_ERROR, RESET_STUCK, CARD_REJECT};
    size_t f, i;
    uint8_t block[512];
    for (f = 0; f < sizeof(faults) / sizeof(faults[0]); ++f) {
        setup(false);
        initialize();
        bus.irqs = 55; /* prepare must discard stale completion. */
        bus.fault = faults[f];
        memset(block, 0xdd, sizeof(block));
        assert(bvstk_sd_service_read(&service, 2, block, &result) != BVSTK_OK);
        assert(!service.info.ready && bus.writes == 0 && !bus.enabled);
        for (i = 0; i < sizeof(block); ++i) assert(block[i] == 0xdd);
        assert(bus.ms < 5300);
        assert(bus.locks == bus.unlocks);
        bus.fault = NONE;
        assert(bvstk_sd_service_read(&service, 2, block, &result) == BVSTK_ERR_NOT_READY);
        initialize();
    }
    setup(false);
    initialize();
    bus.fault = WRONG_DATA;
    bvstk_sd_service_pattern(block, 7, 42);
    assert(bvstk_sd_service_write_verify(&service, 7, block, &result) == BVSTK_ERR_IO);
    assert(result.mismatch_offset == 41 && bus.writes == 1);
    setup(false);
    initialize();
    bus.alias = true;
    assert(bvstk_sd_service_test(&service, 1, 2, 4, &result) == BVSTK_ERR_IO);
    assert(result.lba == 1 && bus.writes == 2 && !service.info.ready);
    setup(false);
    initialize();
    bus.fault = NO_IRQ;
    assert(bvstk_sd_service_write_verify(&service, 7, block, &result) == BVSTK_ERR_TIMEOUT);
    assert(bus.writes == 1 && bus.commands == 1); /* Never retry writes. */
}

static void test_init_and_validation(void)
{
    const enum fault faults[] = {NO_IRQ, NO_COMPLETION, BAD_INIT, STALE_OCR};
    uint8_t bytes[512];
    size_t f;
    uint32_t commands;
    for (f = 0; f < sizeof(faults) / sizeof(faults[0]); ++f) {
        setup(false);
        bus.fault = faults[f];
        assert(bvstk_sd_service_initialize(&service, 20, &result) != BVSTK_OK);
        assert(!service.info.ready && bus.writes == 0 && bus.ms < 130);
    }
    setup(true);
    initialize();
    commands = bus.commands;
    assert(bvstk_sd_service_read(&service, UINT32_MAX / 512U + 1, bytes, &result) == BVSTK_ERR_RANGE);
    assert(bvstk_sd_service_test(&service, UINT32_MAX, 2, 0, &result) == BVSTK_ERR_RANGE);
    assert(bvstk_sd_service_test(&service, 0, 9, 0, &result) == BVSTK_ERR_RANGE);
    assert(bvstk_sd_service_test(&service, 0, 0, 0, &result) == BVSTK_ERR_RANGE);
    assert(bvstk_sd_service_read(&service, 0, NULL, &result) == BVSTK_ERR_MALFORMED);
    assert(bus.commands == commands && bus.writes == 0 && service.info.ready);
    bus.locked = true;
    assert(bvstk_sd_service_read(&service, 0, bytes, &result) == BVSTK_ERR_BUSY);
    assert(bus.commands == commands);
    bus.locked = false;
    assert(bus.locks == bus.unlocks);
    bus.ms = UINT32_MAX - 2; /* Clock passes the old 32-bit boundary. */
    bus.fault = NO_IRQ;
    assert(bvstk_sd_service_read(&service, 0, bytes, &result) == BVSTK_ERR_TIMEOUT);
    assert(bus.ms > UINT32_MAX && result.hardware.elapsed_ms == 5000);
}

static void test_observed_mailbox_profile(void)
{
    uint8_t bytes[512];
    setup(false);
    initialize();
    bus.idle_reply = true;
    assert(bvstk_sd_service_read(&service, 0, bytes, &result) == BVSTK_ERR_IO);
    initialize();
    driver.config.read_mailbox_is_stream = true;
    assert(bvstk_sd_service_read(&service, 0, bytes, &result) == BVSTK_OK);
    assert(result.hardware.read_idle_reply && result.hardware.guards_ok);
    bus.fault = BAD_CRC;
    assert(bvstk_sd_service_read(&service, 0, bytes, &result) == BVSTK_ERR_IO);
    bus.fault = NONE;
    initialize();
    assert(bvstk_sd_service_write_verify(&service, 0, bytes, &result) == BVSTK_ERR_IO);
    assert(result.hardware.command == 24); /* Never relax write/status R1. */
}

static void test_isolation_profile(void)
{
    uint8_t bytes[512];
    uint32_t commands, writes;
    setup(false);
    service.reinitialize_between_commands = true;
    bus.require_isolation = true;
    initialize();
    assert(bvstk_sd_service_test(&service, 4, 8, 123, &result) == BVSTK_OK);
    assert(result.reinitializations == 15 && bus.resets == 16);
    assert(bvstk_sd_service_card_status(&service, &result) == BVSTK_OK);
    assert(bvstk_sd_service_read(&service, 4, bytes, &result) == BVSTK_OK);
    assert(result.reinitializations == 1);
    commands = bus.commands;
    writes = bus.writes;
    bus.fault = NO_IRQ;
    assert(bvstk_sd_service_write_verify(&service, 4, bytes, &result) == BVSTK_ERR_TIMEOUT);
    assert(bus.commands == commands && bus.writes == writes && !service.info.ready);
    bus.fault = NONE;
    assert(bvstk_sd_service_read(&service, 4, bytes, &result) == BVSTK_ERR_NOT_READY);
    assert(bus.commands == commands); /* No implicit recovery after an error. */
    initialize();
    assert(bvstk_sd_service_read(&service, 4, bytes, &result) == BVSTK_OK);
    bus.sdsc = true; /* OCR changes during preparation: do NOT send a write. */
    commands = bus.commands;
    assert(bvstk_sd_service_write_verify(&service, 4, bytes, &result) == BVSTK_ERR_IO);
    assert(bus.commands == commands && !service.info.ready);
    assert(bvstk_sd_service_initialize(&service, 20, &result) == BVSTK_ERR_UNSUPPORTED);
    assert(!service.info.ready && bus.commands == commands);
    setup(false);
    initialize();
    driver.config.read_descriptor_bytes = bus.read_descriptor_bytes = 16;
    assert(bvstk_sd_service_read(&service, 4, bytes, &result) == BVSTK_OK);
}

static void test_filesystem_claim(void)
{
    uint8_t bytes[512], received[512];
    uint32_t commands, writes, resets;
    setup(false);
    assert(bvstk_sd_service_disk_read(&service, 0, bytes, &result) == BVSTK_ERR_BUSY);
    service.reinitialize_between_commands = true;
    bus.require_isolation = true;
    assert(bvstk_sd_service_disk_claim(&service, &result) == BVSTK_OK);
    assert(service.info.ready && service.info.filesystem_owned && bus.writes == 0);
    commands = bus.commands; resets = bus.resets;
    assert(bvstk_sd_service_initialize(&service, 20, &result) == BVSTK_ERR_BUSY);
    assert(bvstk_sd_service_disk_claim(&service, &result) == BVSTK_ERR_BUSY);
    assert(bvstk_sd_service_read(&service, 0, bytes, &result) == BVSTK_ERR_BUSY);
    assert(bvstk_sd_service_write_verify(&service, 0, bytes, &result) == BVSTK_ERR_BUSY);
    assert(bvstk_sd_service_test(&service, 0, 1, 42, &result) == BVSTK_ERR_BUSY);
    assert(bvstk_sd_service_block_length(&service, &result) == BVSTK_ERR_BUSY);
    assert(bvstk_sd_service_card_status(&service, &result) == BVSTK_ERR_BUSY);
    assert(bus.commands == commands && bus.resets == resets && bus.writes == 0);
    bvstk_sd_service_pattern(bytes, 3, 42);
    assert(bvstk_sd_service_disk_write(&service, 3, bytes, &result) == BVSTK_OK);
    assert(bvstk_sd_service_disk_read(&service, 3, received, &result) == BVSTK_OK);
    assert(memcmp(bytes, received, sizeof(bytes)) == 0);
    bus.fault = BAD_CRC;
    writes = bus.writes;
    assert(bvstk_sd_service_disk_write(&service, 3, bytes, &result) == BVSTK_ERR_IO);
    assert(bus.writes == writes + 1 && !service.info.ready);
    bus.fault = NONE;
    commands = bus.commands; resets = bus.resets;
    assert(bvstk_sd_service_disk_write(&service, 3, bytes, &result) == BVSTK_ERR_NOT_READY);
    assert(bvstk_sd_service_disk_read(&service, 3, bytes, &result) == BVSTK_ERR_NOT_READY);
    assert(bvstk_sd_service_disk_claim(&service, &result) == BVSTK_ERR_BUSY);
    assert(bus.commands == commands && bus.resets == resets);
    setup(false);
    bus.fault = BAD_INIT;
    assert(bvstk_sd_service_disk_claim(&service, &result) != BVSTK_OK);
    assert(service.info.filesystem_owned && !service.info.ready && bus.writes == 0);
    assert(bus.locks == bus.unlocks);
}

int main(void)
{
    test_vectors();
    test_success(false);
    test_success(true);
    test_faults();
    test_init_and_validation();
    test_observed_mailbox_profile();
    test_isolation_profile();
    test_filesystem_claim();
    puts("SD master/service contract tests passed");
    return 0;
}
