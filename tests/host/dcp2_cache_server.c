/* Portable CACHE self-tests and an isolated TCP fixture for the Python client. */
#define _POSIX_C_SOURCE 200809L
#include "protocols/dcp2/bvstk_dcp2_cache.h"
#include "protocols/dcp2/bvstk_dcp2_codec.h"
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static bvstk_cache_registry_t registry;
static bvstk_dcp2_cache_session_t session;
static bvstk_cache_snapshot_t snapshot;
static unsigned depth;
static void enter(void *ctx) { (void)ctx; ++depth; }
static void leave(void *ctx) { (void)ctx; assert(depth); --depth; }
static void describe(void *ctx, unsigned i, bvstk_cache_entry_t *e)
{
    (void)ctx; assert(depth);
    (void)snprintf(e->id, sizeof(e->id), "resource_%03u", i);
    e->width_bits = 64; e->valid = i != 1; e->source = i == 2 ? 1 : 2;
    e->value = UINT64_C(0xFEDCBA9876543200) + i; e->updated_ms = 123;
}
static uint16_t setup(void)
{
    bvstk_cache_guard_t guard = {NULL, enter, leave};
    uint16_t handle = 0;
    bvstk_cache_init(&registry, &guard);
    assert(bvstk_cache_define(&registry, &handle, "future_bus", "sensor", 256, 1, describe, NULL) == BVSTK_OK);
    bvstk_dcp2_cache_init(&session, &registry);
    return handle;
}
static size_t target(uint8_t *b, const char *device, int get, uint32_t token, uint16_t first)
{
    size_t len = strlen(device), n = 1;
    b[0] = 1; b[n++] = 10; memcpy(b + n, "future_bus", 10); n += 10;
    b[n++] = (uint8_t)len; memcpy(b + n, device, len); n += len;
    if (get) { bvstk_dcp2_write_be32(b + n, token); bvstk_dcp2_write_be16(b + n + 4, first); n += 6; }
    return n;
}
static uint16_t call(unsigned op, const uint8_t *b, size_t n, uint64_t now, uint8_t *out, uint16_t *size)
{
    return bvstk_dcp2_cache_request(&session, (uint8_t)op, b, n, now, out, 4090, size);
}
static void self_test(void)
{
    uint16_t h = setup(), n, next, sub, seen;
    uint32_t token, generation;
    uint8_t b[128], out[4090], again[4090];
    size_t size;
    uint64_t cursor;
    bvstk_cache_change_t change;
    assert(bvstk_cache_snapshot(&registry, "future_bus", "sensor", &snapshot, &h, &cursor, 200) == BVSTK_OK);
    assert(snapshot.count == 256 && snapshot.entries[0].value == UINT64_C(0xFEDCBA9876543200));
    assert(!snapshot.entries[1].valid && !snapshot.entries[1].value && !snapshot.entries[1].source);
    assert(snapshot.entries[2].source == BVSTK_CACHE_SOURCE_SETTINGS);
    generation = snapshot.generation;
    /* Same value, even in the same millisecond, is still a fresh observation. */
    assert(bvstk_cache_put(&registry, h, 0, snapshot.entries[0].value, 1, 2, 123, 200) == BVSTK_OK);
    assert(bvstk_cache_next(&registry, h, generation, &cursor, &change) == 1 && change.revision == 1);
    assert(bvstk_cache_put(&registry, h, 0, 0, 0, 0, 0, 201) == BVSTK_OK);
    assert(bvstk_cache_next(&registry, h, generation, &cursor, &change) == 1 && !change.entry.valid);
    assert(bvstk_cache_presence(&registry, h, 2, 202) == BVSTK_OK);
    assert(bvstk_cache_next(&registry, h, generation, &cursor, &change) == 1 && change.kind == BVSTK_CACHE_PRESENCE);

    size = target(b, "sensor", 0, 0, 0);
    assert(call(0x21, b, size, 300, out, &n) == 0);
    assert(n <= 4090 && out[1] == 0);
    sub = bvstk_dcp2_read_be16(out + 2); token = bvstk_dcp2_read_be32(out + 4);
    next = bvstk_dcp2_read_be16(out + 34); seen = bvstk_dcp2_read_be16(out + 32);
    assert(sub && next != 0xFFFF && seen == next);
    memcpy(again, out, n);
    assert(bvstk_cache_put(&registry, h, 200, 99, 1, 2, 301, 301) == BVSTK_OK);
    assert(!bvstk_dcp2_cache_event(&session, 301, out, sizeof(out)));
    assert(call(0x21, b, size, 302, out, &n) == 4); /* Incomplete subscription snapshot. */
    size = target(b, "sensor", 1, token, 0);
    assert(call(0x20, b, size, 303, out, &n) == 0 && !memcmp(out, again, n));
    /* Fetching only the last empty page cannot bypass initial synchronization. */
    size = target(b, "sensor", 1, token, 256);
    assert(call(0x20, b, size, 304, out, &n) == 0);
    assert(!bvstk_dcp2_cache_event(&session, 304, out, sizeof(out)));
    while (next != 0xFFFF) {
        size = target(b, "sensor", 1, token, next);
        assert(call(0x20, b, size, 305, out, &n) == 0);
        assert(bvstk_dcp2_read_be16(out + 30) == seen);
        seen += bvstk_dcp2_read_be16(out + 32); next = bvstk_dcp2_read_be16(out + 34);
    }
    assert(seen == 256 && session.snapshot.entries[200].value != 99);
    assert(bvstk_dcp2_cache_event(&session, 306, out, sizeof(out)) && out[1] == 1);
    assert(!bvstk_dcp2_cache_event(&session, 306, out, sizeof(out)));
    for (unsigned i = 0; i <= BVSTK_CACHE_JOURNAL_SIZE; ++i)
        assert(bvstk_cache_put(&registry, h, 0, i, 1, 2, 307, 307) == BVSTK_OK);
    assert(bvstk_dcp2_cache_event(&session, 307, out, sizeof(out)) == 28 && out[1] == 3 && out[25] == 1);
    assert(!bvstk_dcp2_cache_event(&session, 308, out, sizeof(out)));
    size = target(b, "sensor", 0, 0, 0);
    assert(call(0x21, b, size, 400, out, &n) == 0);
    token = bvstk_dcp2_read_be32(out + 4);
    /* An abandoned snapshot gives an explicit event and a stale-token error. */
    assert(bvstk_dcp2_cache_event(&session, 30400, out, sizeof(out)) == 28 && out[25] == 3);
    size = target(b, "sensor", 1, token, 1);
    assert(call(0x20, b, size, 30401, out, &n) == 0x201);
    assert(bvstk_cache_define(&registry, &h, "future_bus", "sensor", 1, 1, describe, NULL) == BVSTK_OK);
    size = target(b, "sensor", 0, 0, 0);
    assert(call(0x21, b, size, 31000, out, &n) == 0);
    assert(bvstk_cache_define(&registry, &h, "future_bus", "sensor", 1, 1, describe, NULL) == BVSTK_OK);
    assert(bvstk_dcp2_cache_event(&session, 31001, out, sizeof(out)) == 28 && out[25] == 2);
    b[0] = 1; bvstk_dcp2_write_be16(b + 1, sub);
    assert(call(0x22, b, 3, 31002, out, &n) == 0 && !n);
    assert(call(0x22, b, 3, 31002, out, &n) == 0x200);
    for (size_t cut = 0; cut < 19; ++cut) {
        size = target(b, "sensor", 1, 0, 0);
        assert(call(0x20, b, cut, 32000, out, &n) != 0);
    }
    assert(call(0x23, b, size, 32000, out, &n) == 2);
    size = target(b, "absent", 1, 0, 0);
    assert(call(0x20, b, size, 32000, out, &n) == 0x200);
    (void)setup();
    for (unsigned i = 0; i < 9; ++i) {
        char id[32];
        uint16_t extra = 0;
        (void)snprintf(id, sizeof(id), "device_%u", i);
        assert(bvstk_cache_define(&registry, &extra, "future_bus", id, 1, 0, describe, NULL) == BVSTK_OK);
        size = target(b, id, 0, 0, 0);
        assert(call(0x21, b, size, 0, out, &n) == (i < 8 ? 0 : 4));
    }
    size = target(b, "device_0", 0, 0, 0);
    assert(call(0x21, b, size, 1, out, &n) == 0); /* Replace while all slots are used. */
    b[3] = '/';
    assert(call(0x21, b, size, 2, out, &n) == 1);
    assert(!depth);
}
static int exact(int fd, uint8_t *b, size_t n, int write_data)
{
    while (n) {
        ssize_t k = write_data ? send(fd, b, n, 0) : recv(fd, b, n, 0);
        if (k <= 0) return -1;
        b += k; n -= (size_t)k;
    }
    return 0;
}
int main(void)
{
    struct sockaddr_in addr = {0};
    socklen_t length = sizeof(addr);
    int listener, fd;
    self_test(); signal(SIGPIPE, SIG_IGN);
    listener = socket(AF_INET, SOCK_STREAM, 0); assert(listener >= 0);
    addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(!bind(listener, (struct sockaddr *)&addr, sizeof(addr)) && !listen(listener, 1));
    assert(!getsockname(listener, (struct sockaddr *)&addr, &length));
    printf("%u\n", ntohs(addr.sin_port)); fflush(stdout);
    while ((fd = accept(listener, NULL, NULL)) >= 0) {
        uint16_t handle = setup();
        uint64_t now = 1000;
        for (;;) {
            uint8_t frame[4104], response[4104], out[4090];
            uint16_t n, status, payload;
            size_t sent;
            bvstk_dcp2_request_t req;
            if (exact(fd, frame, 8, 0)) break;
            payload = bvstk_dcp2_read_be16(frame + 6);
            if (payload < 4 || payload > 4096 || exact(fd, frame + 8, payload, 0)) break;
            assert(bvstk_dcp2_decode_request(frame, payload + 8, &req) == 0);
            status = bvstk_dcp2_cache_request(&session, req.opcode, req.body, req.body_size, now++, out, sizeof(out), &n);
            assert(bvstk_dcp2_encode_response(response, sizeof(response), &req, status, out, n, &sent) == 0);
            if (exact(fd, response, sent, 1)) break;
            /* Update after every request, including intermediate snapshot pages. */
            assert(bvstk_cache_put(&registry, handle, 200, now, 1, 2, now, now) == BVSTK_OK);
            while ((n = bvstk_dcp2_cache_event(&session, now, out, sizeof(out))) != 0) {
                memcpy(response, "DCP2", 4); bvstk_dcp2_write_be16(response + 4, 2);
                bvstk_dcp2_write_be16(response + 6, (uint16_t)(n + 4));
                response[8] = 8; response[9] = 0x61; response[10] = response[11] = 0;
                memcpy(response + 12, out, n);
                if (exact(fd, response, n + 12U, 1)) break;
            }
        }
        close(fd);
    }
    close(listener);
    return 0;
}
