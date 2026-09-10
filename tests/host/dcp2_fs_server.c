/* Exercise the production FS handler over TCP using isolated POSIX fixtures. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <time.h>
#include <unistd.h>

#include "protocols/dcp2/bvstk_dcp2_codec.h"
#include "protocols/dcp2/bvstk_dcp2_fs.h"

typedef struct { DIR *dir; char path[2048]; } test_dir_t;
static unsigned open_files, open_dirs;

static bvstk_fs_result_t resolve(void *context, const char *path, char *out, size_t capacity)
{
    const char *colon = strchr(path, ':');
    int n;
    if (!colon) return BVSTK_FS_INVALID;
    if (!strncmp(path, "sd-pl:", 6)) return BVSTK_FS_NOT_READY;
    if (strncmp(path, "flash:", 6) && strncmp(path, "sd:", 3)) return BVSTK_FS_NOT_FOUND;
    n = snprintf(out, capacity, "%s/%.*s/%s", (char *)context, (int)(colon - path), path, colon + 2);
    return n >= 0 && (size_t)n < capacity ? BVSTK_FS_OK : BVSTK_FS_RANGE;
}

static bvstk_fs_result_t convert_stat(const char *path, bvstk_fs_info_t *out)
{
    struct stat st;
    if (lstat(path, &st)) return errno == ENOENT ? BVSTK_FS_NOT_FOUND : BVSTK_FS_IO;
    if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode)) return BVSTK_FS_UNSUPPORTED;
    out->type = S_ISDIR(st.st_mode) ? BVSTK_FS_ENTRY_DIR : BVSTK_FS_ENTRY_FILE;
    out->size = S_ISDIR(st.st_mode) ? 0 : (uint64_t)st.st_size;
    out->attributes = 0;
    out->stamp = (uint32_t)st.st_mtim.tv_nsec ^ (uint32_t)st.st_mtim.tv_sec;
    return BVSTK_FS_OK;
}

static bvstk_fs_result_t stat_path(void *context, const char *path, bvstk_fs_info_t *out)
{
    char native[2048];
    bvstk_fs_result_t rc = resolve(context, path, native, sizeof(native));
    return rc == BVSTK_FS_OK ? convert_stat(native, out) : rc;
}

static size_t volumes(void *context) { (void)context; return 3; }
static bvstk_fs_result_t volume(void *context, size_t index, bvstk_fs_volume_t *out)
{
    static const char *names[] = { "flash", "sd", "sd-pl" };
    (void)context;
    if (index >= 3) return BVSTK_FS_RANGE;
    strcpy(out->name, names[index]); out->state = index == 2 ? 0 : 1;
    return BVSTK_FS_OK;
}

static bvstk_fs_result_t file_open(void *context, const char *path, void **out, uint64_t *size)
{
    char native[2048];
    struct stat st;
    FILE *file;
    bvstk_fs_result_t rc = resolve(context, path, native, sizeof(native));
    *out = NULL;
    if (rc != BVSTK_FS_OK) return rc;
    file = fopen(native, "rb");
    if (!file) return BVSTK_FS_NOT_FOUND;
    if (fstat(fileno(file), &st)) { fclose(file); return BVSTK_FS_IO; }
    *size = (uint64_t)st.st_size; *out = file; ++open_files;
    return BVSTK_FS_OK;
}

static bvstk_fs_result_t file_read(void *context, void *handle, uint8_t *data, size_t capacity, size_t *size)
{
    (void)context;
    *size = fread(data, 1, capacity, handle);
    return ferror(handle) ? BVSTK_FS_IO : BVSTK_FS_OK;
}
static void file_close(void *context, void *handle)
{
    (void)context; fclose(handle); assert(open_files); --open_files;
}
static bvstk_fs_result_t dir_open(void *context, const char *path, void **out)
{
    test_dir_t *dir = calloc(1, sizeof(*dir));
    bvstk_fs_result_t rc;
    if (!dir) return BVSTK_FS_BUSY;
    rc = resolve(context, path, dir->path, sizeof(dir->path));
    if (rc != BVSTK_FS_OK) { free(dir); return rc; }
    dir->dir = opendir(dir->path);
    if (!dir->dir) { free(dir); return BVSTK_FS_NOT_FOUND; }
    *out = dir; ++open_dirs;
    return BVSTK_FS_OK;
}
static bvstk_fs_result_t dir_read(void *context, void *handle, char *name, size_t capacity, bvstk_fs_info_t *out)
{
    test_dir_t *dir = handle;
    struct dirent *entry;
    char path[4096];
    (void)context;
    do { errno = 0; entry = readdir(dir->dir); }
    while (entry && (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")));
    if (!entry) { name[0] = '\0'; return errno ? BVSTK_FS_IO : BVSTK_FS_OK; }
    if (strlen(entry->d_name) >= capacity) return BVSTK_FS_RANGE;
    strcpy(name, entry->d_name);
    (void)snprintf(path, sizeof(path), "%s/%s", dir->path, name);
    return convert_stat(path, out);
}
static void dir_close(void *context, void *handle)
{
    test_dir_t *dir = handle;
    (void)context; closedir(dir->dir); free(dir); assert(open_dirs); --open_dirs;
}

static uint16_t call(bvstk_dcp2_fs_session_t *s, unsigned op, const uint8_t *body, size_t size,
                      uint32_t time, uint8_t *out, uint16_t *out_size)
{
    return bvstk_dcp2_fs_request(s, (uint8_t)op, body, size, time, out, 4090, out_size);
}

static void self_test(const bvstk_fs_backend_t *backend)
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
    bvstk_dcp2_fs_init(s, backend);
    assert(call(s, 0, NULL, 0, 0, out, &n) == 0 && n > 20);
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
    assert(n == duplicate_size && !memcmp(out, duplicate, n) && open_files == 1);
    body[5] = 1;
    assert(call(s, 2, body, 10 + strlen(path), UINT32_MAX - 98, out, &n) == 1);
    body[5] = 0;
    bvstk_dcp2_write_be32(body, 2);
    assert(call(s, 2, body, 10 + strlen(path), UINT32_MAX - 97, out, &n) == 0);
    bvstk_dcp2_write_be32(body, 3);
    assert(call(s, 2, body, 10 + strlen(path), UINT32_MAX - 96, out, &n) == 4);
    bvstk_dcp2_fs_expire(s, 100); /* Across the millisecond-counter wrap. */
    assert(open_files == 2);
    bvstk_dcp2_write_be32(body, first); bvstk_dcp2_write_be32(body + 4, 0);
    assert(call(s, 3, body, 8, 101, out, &n) == 0 && n == 49);
    assert(call(s, 3, body, 8, 102, duplicate, &duplicate_size) == 0);
    assert(n == duplicate_size && !memcmp(out, duplicate, n));
    bvstk_dcp2_write_be32(body + 4, 2);
    assert(call(s, 3, body, 8, 103, out, &n) == 6);
    bvstk_dcp2_fs_expire(s, 30104);
    assert(open_files == 0);
    assert(call(s, 3, body, 8, 30105, out, &n) == BVSTK_DCP2_FS_BAD_HANDLE);
    bvstk_dcp2_write_be32(close_body, first);
    assert(call(s, 4, close_body, 4, 30106, out, &n) == 0 && !n);
    assert(call(s, 4, close_body, 4, 30107, out, &n) == 0);
    bvstk_dcp2_fs_destroy(s);
    assert(!open_files && !open_dirs);
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
    bvstk_fs_backend_t backend = {NULL, volumes, volume, stat_path, file_open, file_read, file_close, dir_open, dir_read, dir_close};
    struct sockaddr_in addr;
    socklen_t address_size = sizeof(addr);
    int listener;
    if (argc != 2) return 2;
    backend.context = argv[1];
    self_test(&backend);
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
        bvstk_dcp2_fs_init(session, &backend);
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
        assert(!open_files && !open_dirs);
        free(session); close(fd);
    }
}
