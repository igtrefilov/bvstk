/* Exercise the production FS handler over TCP using isolated POSIX fixtures. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <unistd.h>

#include "ports/neutrino-zynq7000/storage/bvstk_fs_posix.h"
#include "protocols/dcp2/bvstk_dcp2_codec.h"
#include "protocols/dcp2/bvstk_dcp2_fs.h"

static uint16_t call(bvstk_dcp2_fs_session_t *s, unsigned op, const uint8_t *body, size_t size,
                      uint32_t time, uint8_t *out, uint16_t *out_size)
{
    return bvstk_dcp2_fs_request(s, (uint8_t)op, body, size, time, out, 4090, out_size);
}

static void self_test(bvstk_fs_posix_t *port)
{
    bvstk_dcp2_fs_session_t *s = calloc(1, sizeof(*s));
    uint8_t body[1024] = {0}, out[4090], duplicate[4090], close_body[4];
    uint16_t n, duplicate_size;
    uint32_t first;
    const char *path = "flash:/file.bin";
    char normalized[512];
    assert(s);
    assert(bvstk_fs_validate_path((const uint8_t *)"flash:/a/../b", 13, normalized, sizeof(normalized)) == BVSTK_FS_INVALID);
    assert(bvstk_fs_validate_path((const uint8_t *)"flash:/config/", 14, normalized, sizeof(normalized)) == BVSTK_FS_OK);
    assert(!strcmp(normalized, "flash:/config"));
    bvstk_dcp2_fs_init(s, &port->reader);
    assert(call(s, 0, NULL, 0, 0, out, &n) == 0 && n > 20);
    assert(bvstk_dcp2_read_be32(out + 2) == 0x3fU); /* Reader-only compatibility. */
    assert(call(s, 5, NULL, 0, 0, out, &n) == 2);
    assert(call(s, 0, body, 1, 0, out, &n) == 1);
    assert(call(s, 3, body, 7, 0, out, &n) == 1);
    bvstk_dcp2_write_be32(body, 1);
    body[4] = BVSTK_FS_FILE;
    bvstk_dcp2_write_be16(body + 6, 37);
    bvstk_dcp2_write_be16(body + 8, (uint16_t)strlen(path));
    memcpy(body + 10, path, strlen(path));
    assert(call(s, 2, body, 10 + strlen(path), UINT32_MAX - 100, out, &n) == 0 && n == 14);
    first = bvstk_dcp2_read_be32(out);
    assert(call(s, 2, body, 10 + strlen(path), UINT32_MAX - 99, duplicate, &duplicate_size) == 0);
    assert(n == duplicate_size && !memcmp(out, duplicate, n) && port->files == 1);
    body[5] = 1;
    assert(call(s, 2, body, 10 + strlen(path), UINT32_MAX - 98, out, &n) == 1);
    body[5] = 0;
    bvstk_dcp2_write_be32(body, 2);
    assert(call(s, 2, body, 10 + strlen(path), UINT32_MAX - 97, out, &n) == 0);
    bvstk_dcp2_write_be32(body, 3);
    assert(call(s, 2, body, 10 + strlen(path), UINT32_MAX - 96, out, &n) == 4);
    bvstk_dcp2_fs_expire(s, 100); /* Across the millisecond-counter wrap. */
    assert(port->files == 2);
    bvstk_dcp2_write_be32(body, first); bvstk_dcp2_write_be32(body + 4, 0);
    assert(call(s, 3, body, 8, 101, out, &n) == 0 && n == 49);
    assert(call(s, 3, body, 8, 102, duplicate, &duplicate_size) == 0);
    assert(n == duplicate_size && !memcmp(out, duplicate, n));
    bvstk_dcp2_write_be32(body + 4, 2);
    assert(call(s, 3, body, 8, 103, out, &n) == 6);
    bvstk_dcp2_fs_expire(s, 30104);
    assert(port->files == 0);
    assert(call(s, 3, body, 8, 30105, out, &n) == BVSTK_DCP2_FS_BAD_HANDLE);
    bvstk_dcp2_write_be32(close_body, first);
    assert(call(s, 4, close_body, 4, 30106, out, &n) == 0 && !n);
    assert(call(s, 4, close_body, 4, 30107, out, &n) == 0);
    bvstk_dcp2_fs_destroy(s);
    assert(!port->files && !port->directories);
    free(s);
}

/* FatFs can return a successful, short write when a volume fills up.
 * Exercise the wire-level terminal error against a real short temporary file. */
static bvstk_fs_result_t short_write(void *context, void *file, const uint8_t *data,
                                     size_t size, size_t *written)
{
    bvstk_fs_posix_t *port = context;
    return port->storage.write(context, file, data, size / 2, written);
}

static void test_failed_upload(bvstk_fs_posix_t *port)
{
    const char path[] = "flash:/failed-upload";
    uint8_t begin[15 + sizeof(path) - 1] = {0}, block[20] = {0}, commit[8] = {0}, out[4090];
    uint16_t n;
    bvstk_fs_info_t info;
    bvstk_dcp2_fs_session_t *s = calloc(1, sizeof(*s));
    bvstk_fs_storage_t storage = port->storage;
    storage.write = short_write;
    assert(s);
    bvstk_dcp2_fs_init(s, &port->reader);
    s->storage = &storage;
    bvstk_dcp2_write_be32(begin, 1);
    begin[11] = 4;
    bvstk_dcp2_write_be16(begin + 13, sizeof(path) - 1);
    memcpy(begin + 15, path, sizeof(path) - 1);
    assert(call(s, BVSTK_DCP2_FS_WRITE_BEGIN, begin, sizeof(begin), 0, out, &n) == 0 && n == 4);
    assert(port->files == 1);
    memcpy(block + 4, out, 4);
    memcpy(commit + 4, out, 4);
    memcpy(block + 16, "data", 4);
    bvstk_dcp2_write_be32(block, 2);
    assert(call(s, BVSTK_DCP2_FS_WRITE_BLOCK, block, sizeof(block), 1, out, &n) == BVSTK_DCP2_FS_IO_ERROR);
    assert(call(s, BVSTK_DCP2_FS_WRITE_BLOCK, block, sizeof(block), 2, out, &n) == BVSTK_DCP2_FS_IO_ERROR);
    bvstk_dcp2_write_be32(block, 3);
    assert(call(s, BVSTK_DCP2_FS_WRITE_BLOCK, block, sizeof(block), 3, out, &n) == BVSTK_DCP2_FS_IO_ERROR);
    bvstk_dcp2_write_be32(commit, 4);
    assert(call(s, BVSTK_DCP2_FS_WRITE_COMMIT, commit, sizeof(commit), 4, out, &n) == BVSTK_DCP2_FS_IO_ERROR);
    assert(call(s, BVSTK_DCP2_FS_WRITE_COMMIT, commit, sizeof(commit), 5, out, &n) == BVSTK_DCP2_FS_IO_ERROR);
    bvstk_dcp2_fs_destroy(s);
    assert(!port->files && !port->directories);
    assert(bvstk_fs_stat(storage.reader, path, &info) == BVSTK_FS_NOT_FOUND);
    free(s);
}

static int exact(int fd, uint8_t *data, size_t size, bool writing)
{
    while (size) {
        ssize_t n = writing ? send(fd, data, size, 0) : recv(fd, data, size, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        data += n; size -= (size_t)n;
    }
    return 0;
}

int main(int argc, char **argv)
{
    bvstk_fs_posix_t port;
    char roots[3][BVSTK_FS_PATH_CAPACITY];
    const char *names[] = {"flash", "sd", "sd-pl"};
    bvstk_fs_posix_volume_t volumes[3];
    struct sockaddr_in addr;
    socklen_t address_size = sizeof(addr);
    int listener;
    if (argc != 2) return 2;
    for (size_t i = 0; i < 3; ++i) {
        int n = snprintf(roots[i], sizeof(roots[i]), "%s/%s", argv[1], names[i]);
        assert(n > 0 && (size_t)n < sizeof(roots[i]));
        volumes[i] = (bvstk_fs_posix_volume_t){names[i], roots[i], false};
    }
    assert(bvstk_fs_posix_init(&port, volumes, 3) == BVSTK_FS_OK);
    self_test(&port);
    test_failed_upload(&port);
    signal(SIGPIPE, SIG_IGN);
    listener = socket(AF_INET, SOCK_STREAM, 0); assert(listener >= 0);
    memset(&addr, 0, sizeof(addr)); addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    assert(listen(listener, 4) == 0);
    assert(getsockname(listener, (struct sockaddr *)&addr, &address_size) == 0);
    printf("%u\n", ntohs(addr.sin_port)); fflush(stdout);
    for (;;) {
        int fd = accept(listener, NULL, NULL);
        bvstk_dcp2_fs_session_t *session = calloc(1, sizeof(*session));
        uint8_t input[4104], output[4104], body[4090];
        assert(fd >= 0 && session);
        bvstk_dcp2_fs_init(session, &port.reader);
        session->storage = bvstk_fs_posix_storage(&port);
        for (;;) {
            struct timespec ts;
            bvstk_dcp2_request_t request;
            uint16_t payload, body_size, status;
            size_t frame_size;
            if (exact(fd, input, 8, false)) break;
            payload = bvstk_dcp2_read_be16(input + 6);
            if (payload > 4096 || payload < 4 || exact(fd, input + 8, payload, false)) break;
            if (bvstk_dcp2_decode_request(input, payload + 8U, &request)) break;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            body_size = 0;
            status = request.service == BVSTK_DCP2_SERVICE_FS ?
                bvstk_dcp2_fs_request(session, request.opcode, request.body, request.body_size,
                    (uint32_t)((uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000), body, sizeof(body), &body_size) : 2;
            assert(!bvstk_dcp2_encode_response(output, sizeof(output), &request, status,
                     status ? NULL : body, status ? 0 : body_size, &frame_size));
            if (exact(fd, output, frame_size, true)) break;
        }
        bvstk_dcp2_fs_destroy(session);
        assert(!port.files && !port.directories);
        free(session); close(fd);
    }
}
