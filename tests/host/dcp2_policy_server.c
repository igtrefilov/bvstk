/* Production policy parser/codec self-tests and a fragmented TCP fixture. */
#define _POSIX_C_SOURCE 200809L
#include "protocols/dcp2/bvstk_dcp2_control.h"
#include <assert.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static bvstk_i2c_master_service_t i2c;
static bvstk_i2c_devices_t devices;
static bvstk_i2c_cache_t cache;
static bvstk_i2c_policy_t policy;
static bvstk_mdio_service_t mdio;
static unsigned depth, locks;
static void enter(void *ctx) { (void)ctx; assert(!depth); ++depth; ++locks; }
static void leave(void *ctx) { (void)ctx; assert(depth == 1); --depth; }
static bvstk_status_t lock(void *ctx, uint32_t timeout)
{ (void)timeout; enter(ctx); return BVSTK_OK; }
static void setup(void)
{
    memset(&i2c, 0, sizeof(i2c)); memset(&devices, 0, sizeof(devices));
    memset(&cache, 0, sizeof(cache)); memset(&policy, 0, sizeof(policy));
    memset(&mdio, 0, sizeof(mdio));
    devices.initialized = 1; devices.count = 1;
    strcpy(devices.items[0].name, "sensor");
    devices.items[0].addr_7b = 54; devices.items[0].reg_count = 32;
    devices.items[0].max_value_code = 127;
    cache.initialized = 1; cache.device_count = 1;
    cache.sink.enter = enter; cache.sink.leave = leave;
    cache.valid[0][1] = 1; cache.values[0][1] = 99;
    policy.initialized = 1; policy.device_count = 1;
    i2c.initialized = 1; i2c.devices = &devices; i2c.cache = &cache; i2c.policy = &policy;
    mdio.initialized = 1; mdio.device_count = 1;
    strcpy(mdio.devices[0].name, "phy"); mdio.devices[0].phy_addr = 1;
    mdio.devices[0].reg_count = 16; mdio.devices[0].autopoll_enabled = 1;
    mdio.devices[0].settings_len = 1; mdio.devices[0].settings[0].reg = 3;
    mdio.devices[0].settings[0].val = 55; mdio.poll_due[0] = 999;
    mdio.cache[0][2].value = 123; mdio.cache[0][2].valid = 1;
    mdio.mutex.lock = lock; mdio.mutex.unlock = leave;
}
static bvstk_status_t call(unsigned bus, unsigned op, const uint8_t *data, size_t size,
                            uint8_t *out, size_t capacity, uint16_t *n)
{
    bvstk_dcp2_request_t req = {(uint8_t)bus, (uint8_t)op, 1, data, (uint16_t)size};
    return bvstk_dcp2_process_policy(&i2c, &mdio, &req, out, capacity, n);
}
static void self_test(void)
{
    uint8_t b[8] = {54, 0, 1, 99}, out[1029], before[1029];
    uint16_t n = 99;
    setup();
    assert(call(2, 3, b, 1, out, sizeof(out), &n) == BVSTK_OK && n == 5);
    assert(!memcmp(out, "\0\0\0\0\0", 5));
    assert(call(2, 4, b, 4, NULL, 0, &n) == BVSTK_OK && n == 0);
    b[1] = 1; assert(call(2, 4, b, 4, out, sizeof(out), &n) == BVSTK_OK);
    assert(call(2, 3, b, 1, out, sizeof(out), &n) == BVSTK_OK && n == 9);
    assert(!memcmp(out, "\0\0\1\0\1\1\143\1\143", 9));
    memcpy(before, out, n);
    memset(out, 0xA5, sizeof(out)); n = 99;
    assert(call(2, 3, b, 1, out, 8, &n) == BVSTK_ERR_RANGE && !n && out[0] == 0xA5);
    assert(call(2, 4, b, 4, out, sizeof(out), &n) == BVSTK_OK); /* duplicate */
    assert(call(2, 3, b, 1, out, sizeof(out), &n) == BVSTK_OK && !memcmp(before, out, 9));
    assert(call(2, 5, b, 4, out, sizeof(out), &n) == BVSTK_OK);
    assert(call(2, 5, b, 4, out, sizeof(out), &n) == BVSTK_ERR_RANGE);
    assert(policy.items[0].whitelist_len == 1 && policy.items[0].blacklist_len == 0);
    b[1] = 0; assert(call(2, 6, b, 2, out, sizeof(out), &n) == BVSTK_OK);
    for (unsigned bus = 2; bus <= 3; ++bus) {
        b[0] = bus == 2 ? 54 : 1; b[1] = 0;
        for (unsigned op = 2; op <= 6; ++op) {
            size_t length = op == 3 ? 1 : op == 4 || op == 5 ? (bus == 2 ? 4 : 7) : 2;
            for (size_t cut = 0; cut < length; ++cut)
                assert(call(bus, op, b, cut, out, sizeof(out), &n) == BVSTK_ERR_MALFORMED && !n);
            assert(call(bus, op, b, length + 1, out, sizeof(out), &n) == BVSTK_ERR_MALFORMED);
            if (op != 3) { b[1] = 2;
                assert(call(bus, op, b, length, out, sizeof(out), &n) == BVSTK_ERR_MALFORMED); b[1] = 0; }
        }
        assert(call(bus, 63, b, 0, out, sizeof(out), &n) == BVSTK_ERR_UNSUPPORTED);
        b[0] = 255; assert(call(bus, 3, b, 1, out, sizeof(out), &n) == BVSTK_ERR_RANGE);
        b[0] = 7; assert(call(bus, 3, b, 1, out, sizeof(out), &n) == BVSTK_ERR_RANGE);
    }
    b[0] = 54; b[1] = 0; b[2] = 32; b[3] = 1;
    assert(call(2, 4, b, 4, out, sizeof(out), &n) == BVSTK_ERR_RANGE);
    b[2] = 1; b[3] = 128;
    assert(call(2, 4, b, 4, out, sizeof(out), &n) == BVSTK_ERR_RANGE);
    b[0] = 1; b[1] = 0; b[2] = 16;
    assert(call(3, 4, b, 7, out, sizeof(out), &n) == BVSTK_ERR_RANGE);
    b[2] = 3; b[3] = 0xAB; b[4] = 0xCD; b[5] = 0xFF; b[6] = 0;
    assert(call(3, 4, b, 7, out, sizeof(out), &n) == BVSTK_OK);
    assert(call(3, 3, b, 1, out, sizeof(out), &n) == BVSTK_OK && n == 10);
    assert(!memcmp(out, "\0\0\1\0\0\3\253\0\377\0", 10));
    b[4] = 0xEF; assert(call(3, 4, b, 7, out, sizeof(out), &n) == BVSTK_OK);
    assert(mdio.devices[0].whitelist_len == 1);
    b[1] = 1; assert(call(3, 4, b, 7, out, sizeof(out), &n) == BVSTK_OK);
    b[1] = 0; assert(call(3, 5, b, 7, out, sizeof(out), &n) == BVSTK_OK);
    assert(mdio.devices[0].blacklist_len == 1 && !mdio.devices[0].whitelist_len);
    b[1] = 1; assert(call(3, 2, b, 2, out, sizeof(out), &n) == BVSTK_OK);
    assert(mdio.devices[0].policy == MDIO_POLICY_BLACKLIST);
    assert(mdio.devices[0].settings[0].val == 55 && mdio.devices[0].autopoll_enabled &&
           mdio.poll_due[0] == 999 && mdio.cache[0][2].value == 123 && mdio.cache[0][2].valid);
    assert(cache.values[0][1] == 99 && cache.valid[0][1]);
    /* Both maximum list lengths fit in one response; counts stay 16 bit. */
    policy.items[0].whitelist_len = policy.items[0].blacklist_len = 256;
    b[0] = 54; assert(call(2, 3, b, 1, out, sizeof(out), &n) == BVSTK_OK && n == 1029);
    assert(out[1] == 1 && out[2] == 0 && out[3] == 1 && out[4] == 0);
    b[1] = 0; b[2] = 1; b[3] = 42;
    assert(call(2, 4, b, 4, out, sizeof(out), &n) == BVSTK_ERR_RANGE);
    mdio.devices[0].whitelist_len = mdio.devices[0].blacklist_len = 64;
    b[0] = 1; assert(call(3, 3, b, 1, out, sizeof(out), &n) == BVSTK_OK && n == 645);
    b[2] = 2; assert(call(3, 4, b, 7, out, sizeof(out), &n) == BVSTK_ERR_RANGE);
    i2c.initialized = 0; b[0] = 54;
    assert(call(2, 3, b, 1, out, sizeof(out), &n) == BVSTK_ERR_BUSY);
    mdio.initialized = 0; b[0] = 1;
    assert(call(3, 3, b, 1, out, sizeof(out), &n) == BVSTK_ERR_BUSY);
    assert(locks && !depth);
}
static int exact(int fd, uint8_t *data, size_t size, int write_data)
{
    while (size) {
        size_t limit = write_data && size > 3 ? 3 : size;
        ssize_t n = write_data ? send(fd, data, limit, 0) : recv(fd, data, limit, 0);
        if (n <= 0) return -1;
        data += n; size -= (size_t)n;
    }
    return 0;
}
int main(void)
{
    struct sockaddr_in address = {0};
    socklen_t length = sizeof(address);
    int listener, fd;
    self_test(); signal(SIGPIPE, SIG_IGN);
    listener = socket(AF_INET, SOCK_STREAM, 0); assert(listener >= 0);
    address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(!bind(listener, (struct sockaddr *)&address, sizeof(address)) && !listen(listener, 1));
    assert(!getsockname(listener, (struct sockaddr *)&address, &length));
    printf("%u\n", ntohs(address.sin_port)); fflush(stdout);
    while ((fd = accept(listener, NULL, NULL)) >= 0) {
        setup();
        for (;;) {
            uint8_t frame[4104], response[4104], out[4090];
            uint8_t event[] = {'D','C','P','2',0,2,0,4,6,0x50,0,0};
            uint16_t payload, n; size_t sent;
            bvstk_status_t status; bvstk_dcp2_request_t request;
            if (exact(fd, frame, 8, 0)) break;
            payload = bvstk_dcp2_read_be16(frame + 6);
            if (payload < 4 || payload > 4096 || exact(fd, frame + 8, payload, 0)) break;
            if (bvstk_dcp2_decode_request(frame, payload + 8U, &request)) break;
            status = bvstk_dcp2_process_policy(&i2c, &mdio, &request, out, sizeof(out), &n);
            assert(!bvstk_dcp2_encode_response(response, sizeof(response), &request,
                bvstk_dcp2_status_from_bvstk(status), n ? out : NULL, n, &sent));
            if (exact(fd, event, sizeof(event), 1) || exact(fd, response, sent, 1)) break;
        }
        close(fd);
    }
    close(listener); return 0;
}
