#define _POSIX_C_SOURCE 200809L
#include "protocols/dcp2/bvstk_dcp2_session.h"
#include "ports/neutrino-zynq7000/storage/bvstk_fs_posix.h"
#include "services/observation/bvstk_observation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bvstk_dcp2_session_t session;
static bvstk_dcp2_adapter_t adapter;
static bvstk_fs_posix_t port;
static bvstk_observation_t observation;
static bvstk_cache_registry_t catalog;
static bvstk_stream_sim_t streams;
static uint8_t replies[128][4104];
static size_t reply_sizes[128], reply_count;
static unsigned locked, hardware_reads, hardware_writes;
static bool disconnected;
static uint64_t now = 1000U;
static uint32_t memory[8];
uint64_t bvstk_platform_now_ms(void) { return now; }
void bvstk_platform_sleep_ms(uint32_t ms) { (void)ms; }
int bvstk_mmio_region_open(bvstk_mmio_region_t *r, uintptr_t a, size_t n)
{ (void)r; (void)a; (void)n; assert(!"unexpected MMIO mapping"); return -1; }
void bvstk_mmio_region_close(bvstk_mmio_region_t *r) { (void)r; }
int bvstk_mmio_read32(const bvstk_mmio_region_t *r, size_t o, uint32_t *v)
{ (void)r; (void)o; (void)v; assert(!"unexpected MMIO read"); return -1; }
int bvstk_mmio_write32(const bvstk_mmio_region_t *r, size_t o, uint32_t v)
{ (void)r; (void)o; (void)v; assert(!"unexpected MMIO write"); return -1; }
static bvstk_status_t check(void *c, uint32_t a, size_t span, unsigned w, int writing)
{ (void)c; (void)writing; return w == 32U && a >= 0x1000U && (uint64_t)a + span <= 0x1020U ? BVSTK_OK : BVSTK_ERR_RANGE; }
static bvstk_status_t read_memory(void *c, uint32_t a, unsigned w, uint64_t *v)
{ (void)c; assert(w == 32U); ++hardware_reads; *v = memory[(a - 0x1000U) / 4U]; return BVSTK_OK; }
static bvstk_status_t write_memory(void *c, uint32_t a, unsigned w, uint64_t v)
{ (void)c; assert(w == 32U); ++hardware_writes; memory[(a - 0x1000U) / 4U] = (uint32_t)v; return BVSTK_OK; }
static void lock(void *c, unsigned scope) { (void)c; if (scope) ++locked; }
static void unlock(void *c, unsigned scope) { (void)c; if (scope) { assert(locked); --locked; } }
static int send_reply(void *c, const void *data, size_t size)
{
    (void)c; assert(!locked);
    if (disconnected) return -1;
    assert(reply_count < 128U && size <= sizeof(replies[0]));
    memcpy(replies[reply_count], data, size); reply_sizes[reply_count++] = size;
    return 0;
}
static uint64_t head(void *c, unsigned bus, unsigned channel)
{ (void)c; return bvstk_observation_head(&observation, bus, channel); }
static void read_observations(void *c, unsigned bus, unsigned channel,
    uint64_t after, bvstk_journal_batch_t *out)
{ (void)c; bvstk_observation_read(&observation, bus, channel, -1, -1, after, out); }
static uint16_t request(uint8_t service, uint8_t op, const uint8_t *body, size_t size)
{
    uint8_t frame[4104] = "DCP2";
    assert(size <= BVSTK_DCP2_MAX_PAYLOAD - 4U);
    bvstk_dcp2_write_be16(frame + 4U, 2);
    bvstk_dcp2_write_be16(frame + 6U, (uint16_t)(size + 4U));
    frame[8] = service; frame[9] = op; bvstk_dcp2_write_be16(frame + 10U, 17U);
    if (size) memcpy(frame + 12U, body, size);
    reply_count = 0;
    assert(!bvstk_dcp2_session_frame(&session, frame, size + 12U, now));
    assert(reply_count == 1 && reply_sizes[0] >= 14U);
    assert(replies[0][8] == service && replies[0][9] == (uint8_t)((op & 0x3FU) | 0x80U));
    assert(bvstk_dcp2_read_be16(replies[0] + 10U) == 17U);
    return bvstk_dcp2_read_be16(replies[0] + 12U);
}
static void publish(unsigned type, unsigned source, unsigned bus)
{
    bvstk_event_t e = {0}; e.type = (uint16_t)type; e.source = (uint8_t)source;
    e.bus = (uint8_t)bus; e.operation = BVSTK_EVENT_OP_WRITE; e.time_us = now * 1000U;
    e.arg0 = 1; e.arg1 = 3; e.arg2 = 0x55;
    bvstk_observation_publish(&observation, &e);
}
static uint32_t open_file(void)
{
    uint8_t body[64] = {0};
    const char path[] = "flash:/data.bin";
    bvstk_dcp2_write_be32(body, 123U); body[4] = BVSTK_FS_FILE;
    bvstk_dcp2_write_be16(body + 6U, 7U);
    bvstk_dcp2_write_be16(body + 8U, sizeof(path) - 1U);
    memcpy(body + 10U, path, sizeof(path) - 1U);
    assert(!request(7, BVSTK_DCP2_FS_OPEN, body, 10U + sizeof(path) - 1U));
    return bvstk_dcp2_read_be32(replies[0] + 14U);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    char flash[512], sd[512];
    assert(snprintf(flash, sizeof(flash), "%s/flash", argv[1]) < (int)sizeof(flash));
    assert(snprintf(sd, sizeof(sd), "%s/sd", argv[1]) < (int)sizeof(sd));
    assert(!mkdir(argv[1], 0700) && !mkdir(flash, 0700) && !mkdir(sd, 0700));
    const bvstk_fs_posix_volume_t volumes[] = {{"flash", flash, false}, {"sd", sd, false}, {"sd-pl", NULL, false}};
    assert(bvstk_fs_posix_init(&port, volumes, 3) == BVSTK_FS_OK);
    const bvstk_fs_storage_t *storage = bvstk_fs_posix_storage(&port);
    bvstk_fs_writer_t writer;
    assert(bvstk_fs_writer_open(&writer, storage, "flash:/data.bin", false) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_write(&writer, "0123456789", 10) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_close(&writer) == BVSTK_FS_OK);
    const bvstk_memory_t mem = {NULL, BVSTK_MEMORY_32, check, read_memory, write_memory};
    bvstk_observation_init(&observation, NULL); bvstk_cache_init(&catalog, NULL);
    bvstk_stream_sim_init(&streams, NULL);
    adapter.control.memory = &mem; adapter.lock = lock; adapter.unlock = unlock;
    adapter.i2c_supported = adapter.mdio_supported = true;
    adapter.files = storage->reader; adapter.cache = &catalog; adapter.streams = &streams;
    adapter.head = head; adapter.read = read_observations;
    bvstk_dcp2_session_init(&session, &adapter, send_reply, NULL);
    assert(!request(0, 0, NULL, 0));
    const uint8_t byte = 0;
    assert(request(0, 0x80, NULL, 0) == 1U);
    assert(request(0, 0, &byte, 1) == 1U);
    assert(!request(0, 0, NULL, 0));
    assert(request(2, 0, (const uint8_t[]){0x36, 0}, 2) == 4U);
    adapter.i2c_supported = false;
    assert(request(2, 0, (const uint8_t[]){0x36, 0}, 2) == 2U);
    adapter.i2c_supported = true;
    uint8_t mem_body[8] = {0, 32, 0, 0, 0x10, 0, 0, 2};
    memory[0] = 42; memory[1] = 43;
    assert(!request(1, 0, mem_body, sizeof(mem_body)) && hardware_reads == 2);
    assert(bvstk_dcp2_read_be32(replies[0] + 14U) == 42 && bvstk_dcp2_read_be32(replies[0] + 18U) == 42);
    mem_body[0] = 1;
    assert(!request(1, 0, mem_body, sizeof(mem_body)));
    assert(bvstk_dcp2_read_be32(replies[0] + 18U) == 43 && !hardware_writes);
    /* FS state survives unrelated commands, retries do not advance the reader. */
    assert(!request(7, BVSTK_DCP2_FS_INFO, NULL, 0));
    uint32_t handle = open_file(); assert(handle && port.files == 1);
    assert(!request(0, 0, NULL, 0));
    assert(open_file() == handle && port.files == 1);
    uint8_t read_body[8] = {0}; bvstk_dcp2_write_be32(read_body, handle);
    assert(!request(7, BVSTK_DCP2_FS_READ, read_body, sizeof(read_body)));
    assert(reply_sizes[0] == 33U && !memcmp(replies[0] + 26U, "0123456", 7));
    uint8_t repeated[64]; size_t repeated_size = reply_sizes[0]; memcpy(repeated, replies[0], repeated_size);
    assert(!request(7, BVSTK_DCP2_FS_READ, read_body, sizeof(read_body)));
    assert(reply_sizes[0] == repeated_size && !memcmp(repeated, replies[0], repeated_size));
    /* NOTIFY filtering and delivery metadata are independent of bus I/O. */
    uint8_t subscribe[13] = {0};
    bvstk_dcp2_write_be32(subscribe, DCP2_NOTIFY_CLASS_REG_DENIED);
    bvstk_dcp2_write_be32(subscribe + 4U, 1U << BVSTK_EVENT_SOURCE_DCP);
    bvstk_dcp2_write_be32(subscribe + 8U, 1U); subscribe[12] = DCP2_NOTIFY_FLAG_WITH_TIMESTAMP;
    assert(!request(6, 0x10, subscribe, sizeof(subscribe)));
    publish(BVSTK_EVENT_REG_DENIED, BVSTK_EVENT_SOURCE_CONSOLE, 0);
    publish(BVSTK_EVENT_REG_DENIED, BVSTK_EVENT_SOURCE_DCP, 1);
    publish(BVSTK_EVENT_REG_DENIED, BVSTK_EVENT_SOURCE_DCP, 0);
    reply_count = 0; assert(!bvstk_dcp2_session_poll(&session, now));
    assert(reply_count == 1 && replies[0][8] == 6 && replies[0][9] == 0x50U);
    assert(reply_sizes[0] == 40U && bvstk_dcp2_read_be16(replies[0] + 20U) == BVSTK_EVENT_REG_DENIED);
    for (unsigned i = 0; i < BVSTK_JOURNAL_CAPACITY + 8U; ++i)
        publish(BVSTK_EVENT_REG_DENIED, BVSTK_EVENT_SOURCE_CONSOLE, 0);
    reply_count = 0; assert(!bvstk_dcp2_session_poll(&session, now));
    assert(reply_count == 1 && bvstk_dcp2_read_be16(replies[0] + 20U) == DCP2_NOTIFY_EV_LOST);
    assert(!request(6, 0x11, NULL, 0));
    publish(BVSTK_EVENT_REG_DENIED, BVSTK_EVENT_SOURCE_DCP, 0);
    reply_count = 0; assert(!bvstk_dcp2_session_poll(&session, now) && !reply_count);
    /* Synthetic streams require explicit local configuration and mark their source. */
    assert(bvstk_stream_sim_configure(&streams, (uint32_t)now, 4, true, 10,
        DCP2_STREAM_SIM_PATTERN_RAMP, 8));
    const uint8_t stream_flags = 3;
    assert(!request(4, 0x10, &stream_flags, 1));
    reply_count = 0; now += 10;
    assert(!bvstk_dcp2_session_poll(&session, now) && reply_count == 1);
    assert(replies[0][8] == 4 && replies[0][9] == 0x50U && replies[0][20] == 4U);
    assert(bvstk_dcp2_read_be16(replies[0] + 25U) == 32U);
    assert(bvstk_dcp2_read_be32(replies[0] + 27U) == 1U);
    reply_count = 0; now += 50;
    assert(!bvstk_dcp2_session_poll(&session, now) && reply_count == 1);
    assert((replies[0][20] & 5U) == 5U && bvstk_dcp2_read_be32(replies[0] + 21U) == 4U);
    /* Disconnect discards every subscription, snapshot and file handle. */
    assert(port.files == 1);
    bvstk_dcp2_session_destroy(&session); assert(!port.files && !port.directories);
    bvstk_dcp2_session_init(&session, &adapter, send_reply, NULL);
    reply_count = 0; now += 10;
    assert(!bvstk_dcp2_session_poll(&session, now) && !reply_count);
    handle = open_file(); assert(handle && port.files == 1);
    now += BVSTK_DCP2_FS_IDLE_MS + 1U;
    assert(!bvstk_dcp2_session_poll(&session, now) && !port.files);
    assert(request(7, BVSTK_DCP2_FS_READ, read_body, sizeof(read_body)) == BVSTK_DCP2_FS_BAD_HANDLE);
    (void)open_file(); disconnected = true;
    uint8_t frame[12] = {'D','C','P','2',0,2,0,4,0,0,0,1};
    assert(bvstk_dcp2_session_frame(&session, frame, sizeof(frame), now) < 0);
    bvstk_dcp2_session_destroy(&session); assert(!port.files && !port.directories);
    assert(bvstk_fs_remove(storage, "flash:/data.bin") == BVSTK_FS_OK);
    assert(bvstk_fs_posix_destroy(&port) == BVSTK_FS_OK);
    assert(!rmdir(flash) && !rmdir(sd) && !rmdir(argv[1]));
    puts("Common DCP2 session: dispatch, MEM, FS retries/expiry, filtered NOTIFY/loss, synthetic streams and disconnect cleanup passed");
    return 0;
}
