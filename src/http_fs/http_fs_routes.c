#include "../http/http_server.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "lwip/sockets.h"

#include "xstatus.h"

#include "../fs/fs_devices.h"
#include "../fs/fs_shared.h"
#include "../tar/tar.h"

static int ctx_lock(const fs_shared_ctx_t *ctx)
{
    if (!ctx || !ctx->mutex || !*(ctx->mutex)) return 1;
    return xSemaphoreTake(*(ctx->mutex), pdMS_TO_TICKS(2000)) == pdTRUE;
}

static void ctx_unlock(const fs_shared_ctx_t *ctx)
{
    if (ctx && ctx->mutex && *(ctx->mutex)) xSemaphoreGive(*(ctx->mutex));
}

static void http_write_str(int fd, const char *s)
{
    (void)lwip_write(fd, s, (int)strlen(s));
}

static void http_reply_simple(int fd, int code, const char *reason, const char *body)
{
    char hdr[256];
    const char *b = body ? body : "";
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 %d %s\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %u\r\n"
        "\r\n",
        code, reason ? reason : "ERR", (unsigned)strlen(b));
    if (n > 0) lwip_write(fd, hdr, n);
    if (b[0]) http_write_str(fd, b);
}

static int is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0;
}

static bool url_decode_inplace(char *s)
{
    if (!s) return false;
    char *w = s;
    for (char *p = s; *p; ++p) {
        if (*p == '%' && is_hex_digit((char)p[1]) && is_hex_digit((char)p[2])) {
            int v = (hex_val((char)p[1]) << 4) | hex_val((char)p[2]);
            *w++ = (char)v;
            p += 2;
            continue;
        }
        if (*p == '+') { *w++ = ' '; continue; }
        *w++ = *p;
    }
    *w = '\0';
    return true;
}

static bool starts_with(const char *s, const char *pfx)
{
    return s && pfx && strncmp(s, pfx, strlen(pfx)) == 0;
}

static bool map_url_to_fatfs(const char *url_path, char *out_fat, size_t out_sz,
                             const fs_device_info_t **out_dev, bool *out_tar_mode)
{
    if (!url_path || !out_fat || out_sz == 0) return false;
    if (out_tar_mode) *out_tar_mode = false;

    const char *p = url_path;
    if (*p != '/') return false;
    p++;

    bool tar_mode = false;
    if (starts_with(p, "tar/")) {
        tar_mode = true;
        p += 4;
    }

    const fs_device_info_t *dev = NULL;
    const char *dev_name = NULL;
    if (starts_with(p, "sd")) {
        dev_name = "sd";
        p += 2;
    } else if (starts_with(p, "flash")) {
        dev_name = "flash";
        p += 5;
    } else {
        return false;
    }

    dev = fs_device_by_name(dev_name);
    if (!dev || !dev->ctx || !dev->ctx->root) return false;
    if (out_dev) *out_dev = dev;
    if (out_tar_mode) *out_tar_mode = tar_mode;

    if (*p == '\0') {
        strncpy(out_fat, dev->ctx->root, out_sz - 1);
        out_fat[out_sz - 1] = '\0';
        return true;
    }
    if (*p != '/') return false;
    while (*p == '/') p++;

    char tmp[FS_PATH_MAX * 2];
    strncpy(tmp, p, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    url_decode_inplace(tmp);

    if (strstr(tmp, "..")) return false;
    if (strchr(tmp, ':')) return false;

    size_t root_len = strlen(dev->ctx->root);
    size_t suf_len = strlen(tmp);
    if (root_len + 1 + suf_len + 1 > out_sz) return false;
    strncpy(out_fat, dev->ctx->root, out_sz - 1);
    out_fat[out_sz - 1] = '\0';
    if (root_len && out_fat[root_len - 1] != '/') strncat(out_fat, "/", out_sz - strlen(out_fat) - 1);
    strncat(out_fat, tmp, out_sz - strlen(out_fat) - 1);
    return true;
}

static int tar_sock_write(void *user, const void *buf, size_t len)
{
    int fd = *(int *)user;
    int w = lwip_write(fd, buf, (int)len);
    return (w <= 0) ? -1 : w;
}

static int tar_sock_read(void *user, void *buf, size_t len)
{
    http_conn_t *c = (http_conn_t *)user;
    int r = c->read_body(c->read_user, buf, len);
    return r;
}

static int handle_get_file(int fd, const fs_device_info_t *dev, const char *fat_path)
{
    if (!dev || !dev->ctx) return XST_FAILURE;
    if (fs_device_prepare(dev) != XST_SUCCESS) return XST_FAILURE;
    if (!ctx_lock(dev->ctx)) return XST_FAILURE;

    FILINFO info;
    FRESULT sres = f_stat(fat_path, &info);
    if (sres != FR_OK || (info.fattrib & AM_DIR)) {
        ctx_unlock(dev->ctx);
        http_reply_simple(fd, 404, "Not Found", "not found\r\n");
        return XST_SUCCESS;
    }

    FIL f;
    FRESULT ores = f_open(&f, fat_path, FA_READ);
    if (ores != FR_OK) {
        ctx_unlock(dev->ctx);
        http_reply_simple(fd, 500, "ERR", "open failed\r\n");
        return XST_SUCCESS;
    }

    char hdr[256];
    int hn = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: %lu\r\n"
        "\r\n",
        (unsigned long)info.fsize);
    if (hn > 0) lwip_write(fd, hdr, hn);

    uint8_t buf[1024];
    UINT br = 0;
    while (1) {
        FRESULT rres = f_read(&f, buf, sizeof(buf), &br);
        if (rres != FR_OK) break;
        if (br == 0) break;
        int wr = lwip_write(fd, buf, (int)br);
        if (wr <= 0) break;
    }
    f_close(&f);
    ctx_unlock(dev->ctx);
    return XST_SUCCESS;
}

static int handle_put_file(int fd, const fs_device_info_t *dev, const char *fat_path, http_conn_t *conn)
{
    if (!dev || !dev->ctx || !conn) return XST_FAILURE;
    if (fs_device_prepare(dev) != XST_SUCCESS) return XST_FAILURE;
    if (!ctx_lock(dev->ctx)) return XST_FAILURE;

    FIL f;
    FRESULT ores = f_open(&f, fat_path, FA_WRITE | FA_CREATE_ALWAYS);
    if (ores != FR_OK) {
        ctx_unlock(dev->ctx);
        http_reply_simple(fd, 500, "ERR", "open failed\r\n");
        return XST_SUCCESS;
    }

    uint8_t buf[1024];
    while (1) {
        int r = conn->read_body(conn->read_user, buf, sizeof(buf));
        if (r < 0) { http_reply_simple(fd, 400, "Bad Request", "read body failed\r\n"); break; }
        if (r == 0) { http_reply_simple(fd, 200, "OK", "OK\r\n"); break; }
        UINT bw = 0;
        FRESULT wres = f_write(&f, buf, (UINT)r, &bw);
        if (wres != FR_OK || bw != (UINT)r) { http_reply_simple(fd, 500, "ERR", "write failed\r\n"); break; }
    }
    f_close(&f);
    ctx_unlock(dev->ctx);
    return XST_SUCCESS;
}

static int handle_get_tar(int fd, const fs_device_info_t *dev, const char *fat_dir_path)
{
    if (!dev || !dev->ctx) return XST_FAILURE;
    if (fs_device_prepare(dev) != XST_SUCCESS) return XST_FAILURE;
    if (!ctx_lock(dev->ctx)) return XST_FAILURE;

    char hdr[256];
    int hn = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: application/x-tar\r\n"
        "\r\n");
    if (hn > 0) lwip_write(fd, hdr, hn);

    int u = fd;
    (void)tar_stream_dir_fatfs(dev->ctx, fat_dir_path, tar_sock_write, &u);
    ctx_unlock(dev->ctx);
    return XST_SUCCESS;
}

static int handle_put_tar(int fd, const fs_device_info_t *dev, const char *fat_dir_path, http_conn_t *conn)
{
    if (!dev || !dev->ctx || !conn) return XST_FAILURE;
    if (fs_device_prepare(dev) != XST_SUCCESS) return XST_FAILURE;
    if (!ctx_lock(dev->ctx)) return XST_FAILURE;

    int rc = tar_extract_into_dir_fatfs(dev->ctx, fat_dir_path, tar_sock_read, conn);
    ctx_unlock(dev->ctx);
    if (rc == XST_SUCCESS) http_reply_simple(fd, 200, "OK", "OK\r\n");
    else http_reply_simple(fd, 400, "Bad Request", "tar extract failed\r\n");
    return XST_SUCCESS;
}

int http_handle_request(const http_request_t *req, http_conn_t *conn)
{
    if (!req || !conn || !req->method || !req->path) return 0;

    char fat_path[FS_PATH_MAX * 2];
    const fs_device_info_t *dev = NULL;
    bool tar_mode = false;
    if (!map_url_to_fatfs(req->path, fat_path, sizeof(fat_path), &dev, &tar_mode)) {
        return 0;
    }

    if (strcasecmp(req->method, "GET") == 0) {
        if (tar_mode) (void)handle_get_tar(conn->fd, dev, fat_path);
        else (void)handle_get_file(conn->fd, dev, fat_path);
        return 1;
    }
    if (strcasecmp(req->method, "PUT") == 0) {
        if (!(req->has_content_length || req->chunked)) {
            http_reply_simple(conn->fd, 411, "Length Required", "need Content-Length or chunked\r\n");
            return 1;
        }
        if (tar_mode) (void)handle_put_tar(conn->fd, dev, fat_path, conn);
        else (void)handle_put_file(conn->fd, dev, fat_path, conn);
        return 1;
    }
    http_reply_simple(conn->fd, 405, "Method Not Allowed", "only GET/PUT\r\n");
    return 1;
}

