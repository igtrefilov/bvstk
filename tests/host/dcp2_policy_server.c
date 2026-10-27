/* Production policy parser/codec self-tests and a fragmented TCP fixture. */
#define _POSIX_C_SOURCE 200809L
#include "protocols/dcp2/bvstk_dcp2_control.h"
#include "protocols/dcp2/bvstk_dcp2_policy.h"
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
static bvstk_status_t save_status;
static unsigned saves;
static bvstk_dcp2_policy_session_t profile_session;
static bvstk_status_t save_profile(void *context, uint8_t service, size_t device_id)
{
    assert(context == &saves && (service == 2 || service == 3));
    assert(device_id < (service == 2 ? 4U : 1U));
    ++saves;
    return save_status;
}
static bvstk_status_t catalog(void *context, uint8_t service, size_t id,
                              bvstk_dcp2_policy_catalog_t *out)
{
    static bvstk_profile_metadata_t metadata;
    (void)context;
    memset(&metadata, 0, sizeof(metadata));
    if (service == 2 && id == 3) {
        metadata.version = 2; metadata.ids_size = 1;
        for (unsigned r = 0; r < 8; ++r) {
            metadata.id_offset[r] = metadata.ids_size;
            int n = snprintf(metadata.ids + metadata.ids_size,
                sizeof(metadata.ids) - metadata.ids_size, "a%u_%02x", r < 4 ? 0U : 2U, r % 4);
            metadata.ids_size += (uint16_t)(n + 1);
        }
    }
    return bvstk_dcp2_policy_catalog_from_profile(out, &metadata,
        service == 2 ? devices.items[id].reg_count : mdio.devices[id].reg_count);
}
static unsigned depth, locks;
static void enter(void *ctx) { (void)ctx; assert(!depth); ++depth; ++locks; }
static void leave(void *ctx) { (void)ctx; assert(depth == 1); --depth; }
static bvstk_status_t lock(void *ctx, uint32_t timeout)
{ (void)timeout; enter(ctx); return BVSTK_OK; }
static void setup(void)
{
    saves = 0; save_status = BVSTK_OK;
    memset(&i2c, 0, sizeof(i2c)); memset(&devices, 0, sizeof(devices));
    memset(&cache, 0, sizeof(cache)); memset(&policy, 0, sizeof(policy));
    memset(&mdio, 0, sizeof(mdio));
    devices.initialized = 1; devices.count = 4;
    strcpy(devices.items[0].name, "sensor");
    devices.items[0].addr_7b = 54; devices.items[0].reg_count = 32;
    devices.items[0].max_value_code = 127;
    strcpy(devices.items[1].name, "switch");
    devices.items[1].addr_7b = 0x70; devices.items[1].reg_count = 1;
    devices.items[1].endpoint.router.channel_count = 2;
    devices.items[1].endpoint.router.select_mask = 3;
    devices.items[1].endpoint.router.channels[0] = (bvstk_i2c_channel_t){1, 3};
    devices.items[1].endpoint.router.channels[1] = (bvstk_i2c_channel_t){2, 3};
    for (unsigned i = 2; i < 4; ++i) {
        strcpy(devices.items[i].name, i == 2 ? "rtc" : "sfp");
        strcpy(devices.items[i].endpoint.parent, "switch");
        devices.items[i].endpoint.channel = (uint8_t)(i - 2);
        devices.items[i].addr_7b = i == 2 ? 0x51 : 0x50;
        devices.items[i].reg_count = 8; devices.items[i].max_value_code = 255;
    }
    devices.items[3].endpoint.space_count = 2;
    devices.items[3].endpoint.spaces[0] = (bvstk_address_space_t){"identity", 0, 4, 0x50};
    devices.items[3].endpoint.spaces[1] = (bvstk_address_space_t){"diagnostics", 4, 4, 0x51};
    cache.initialized = 1; cache.device_count = 4;
    cache.sink.enter = enter; cache.sink.leave = leave;
    cache.valid[0][1] = 1; cache.values[0][1] = 99;
    policy.initialized = 1; policy.device_count = 4;
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
    if (op == BVSTK_DCP2_OP_I2C_POLICY_SAVE) {
        *n = 0;
        return bvstk_dcp2_process_policy_save(&i2c, &mdio, &req, save_profile, &saves);
    }
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
        for (unsigned op = 2; op <= 7; ++op) {
            size_t length = op == 3 || op == 7 ? 1 : op == 4 || op == 5 ? (bus == 2 ? 4 : 7) : 2;
            for (size_t cut = 0; cut < length; ++cut)
                assert(call(bus, op, b, cut, out, sizeof(out), &n) == BVSTK_ERR_MALFORMED && !n);
            assert(call(bus, op, b, length + 1, out, sizeof(out), &n) == BVSTK_ERR_MALFORMED);
            if (op != 3 && op != 7) { b[1] = 2;
                assert(call(bus, op, b, length, out, sizeof(out), &n) == BVSTK_ERR_MALFORMED); b[1] = 0; }
        }
        assert(call(bus, 63, b, 0, out, sizeof(out), &n) == BVSTK_ERR_UNSUPPORTED);
        b[0] = 255; assert(call(bus, 3, b, 1, out, sizeof(out), &n) == BVSTK_ERR_RANGE);
        b[0] = 7; assert(call(bus, 3, b, 1, out, sizeof(out), &n) == BVSTK_ERR_RANGE);
        assert(call(bus, 7, b, 1, out, sizeof(out), &n) == BVSTK_ERR_RANGE);
    }
    assert(saves == 0); /* No invalid request may reach storage. */
    for (unsigned bus = 2; bus <= 3; ++bus) {
        b[0] = bus == 2 ? 54 : 1;
        assert(call(bus, 7, b, 1, out, sizeof(out), &n) == BVSTK_OK && !n);
        save_status = BVSTK_ERR_IO;
        assert(call(bus, 7, b, 1, out, sizeof(out), &n) == BVSTK_ERR_IO && !n);
        save_status = BVSTK_ERR_NOT_READY;
        assert(call(bus, 7, b, 1, out, sizeof(out), &n) == BVSTK_ERR_BUSY);
        save_status = BVSTK_OK;
        bvstk_dcp2_request_t req = {(uint8_t)bus, 7, 1, b, 1};
        assert(bvstk_dcp2_process_policy_save(&i2c, &mdio, &req, NULL, NULL) == BVSTK_ERR_UNSUPPORTED);
        req.body = NULL;
        assert(bvstk_dcp2_process_policy_save(&i2c, &mdio, &req, save_profile, &saves) == BVSTK_ERR_MALFORMED);
    }
    assert(saves == 6);
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
    assert(call(2, 7, b, 1, out, sizeof(out), &n) == BVSTK_ERR_BUSY);
    mdio.initialized = 0; b[0] = 1;
    assert(call(3, 3, b, 1, out, sizeof(out), &n) == BVSTK_ERR_BUSY);
    assert(call(3, 7, b, 1, out, sizeof(out), &n) == BVSTK_ERR_BUSY);
    assert(saves == 6);
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
static void profile_self_test(void)
{
    uint8_t body[] = {1,3,'i','2','c',3,'r','t','c',0,0,0,0,0,0};
    uint8_t out[80];
    uint16_t n, first = 0;
    const bvstk_dcp2_policy_adapter_t adapter = {&i2c, &mdio, &saves, catalog, NULL, save_profile};
    setup();
    bvstk_dcp2_policy_init(&profile_session);
    /* Both lists at their maximum size must fit across frozen pages. */
    policy.items[2].whitelist_len = policy.items[2].blacklist_len = 256;
    for (unsigned i = 0; i < 256; ++i) {
        policy.items[2].whitelist[i] = (i2c_rule_entry_t){0, (uint8_t)i};
        policy.items[2].blacklist[i] = (i2c_rule_entry_t){1, (uint8_t)i};
    }
    assert(bvstk_dcp2_policy_request(&profile_session, &adapter, 3, body, sizeof(body),
        10, out, sizeof(out), &n) == 0);
    uint32_t id = bvstk_dcp2_read_be32(out + 2);
    assert(bvstk_dcp2_read_be16(out + 6) == 256 && bvstk_dcp2_read_be16(out + 8) == 256);
    policy.items[2].whitelist_len = 0; /* Concurrent shell edit leaves the snapshot frozen. */
    for (;;) {
        assert(bvstk_dcp2_read_be16(out + 10) == first);
        first += bvstk_dcp2_read_be16(out + 12);
        uint16_t next = bvstk_dcp2_read_be16(out + 14);
        if (next == UINT16_MAX) break;
        assert(next == first);
        bvstk_dcp2_write_be32(body + 9, id);
        bvstk_dcp2_write_be16(body + 13, next);
        assert(bvstk_dcp2_policy_request(&profile_session, &adapter, 3, body, sizeof(body),
            11, out, sizeof(out), &n) == 0);
    }
    assert(first == 512);
    assert(bvstk_dcp2_policy_request(&profile_session, &adapter, 3, body, sizeof(body),
        30012, out, sizeof(out), &n) == BVSTK_DCP2_POLICY_STALE_SNAPSHOT && n == 0);
    body[0] = 2;
    assert(bvstk_dcp2_policy_request(&profile_session, &adapter, 3, body, sizeof(body),
        11, out, sizeof(out), &n) == 2);
}
int main(void)
{
    struct sockaddr_in address = {0};
    socklen_t length = sizeof(address);
    int listener, fd;
    self_test(); profile_self_test(); signal(SIGPIPE, SIG_IGN);
    listener = socket(AF_INET, SOCK_STREAM, 0); assert(listener >= 0);
    address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(!bind(listener, (struct sockaddr *)&address, sizeof(address)) && !listen(listener, 1));
    assert(!getsockname(listener, (struct sockaddr *)&address, &length));
    printf("%u\n", ntohs(address.sin_port)); fflush(stdout);
    while ((fd = accept(listener, NULL, NULL)) >= 0) {
        setup();
        bvstk_dcp2_policy_init(&profile_session);
        for (;;) {
            uint8_t frame[4104], response[4104], out[4090];
            uint8_t event[] = {'D','C','P','2',0,2,0,4,6,0x50,0,0};
            uint16_t payload, n; size_t sent;
            bvstk_status_t status; bvstk_dcp2_request_t request;
            if (exact(fd, frame, 8, 0)) break;
            payload = bvstk_dcp2_read_be16(frame + 6);
            if (payload < 4 || payload > 4096 || exact(fd, frame + 8, payload, 0)) break;
            if (bvstk_dcp2_decode_request(frame, payload + 8U, &request)) break;
            uint16_t wire;
            if (request.service == BVSTK_DCP2_SERVICE_POLICY) {
                const bvstk_dcp2_policy_adapter_t adapter = {&i2c, &mdio, &saves, catalog, NULL, save_profile};
                wire = bvstk_dcp2_policy_request(&profile_session, &adapter, request.opcode,
                    request.body, request.body_size, 10, out, 80, &n);
            } else {
                status = call(request.service, request.opcode, request.body, request.body_size,
                              out, sizeof(out), &n);
                wire = bvstk_dcp2_status_from_bvstk(status);
            }
            assert(!bvstk_dcp2_encode_response(response, sizeof(response), &request,
                wire, n ? out : NULL, n, &sent));
            if (exact(fd, event, sizeof(event), 1) || exact(fd, response, sent, 1)) break;
        }
        close(fd);
    }
    close(listener); return 0;
}
