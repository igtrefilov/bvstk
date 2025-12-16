#include "tar_shell.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "lwip/sockets.h"
#include "xstatus.h"

#include "../../fs/fs_devices.h"
#include "../../tar/tar.h"

#define CONSOLE_PATH_MAX 128

static int ctx_lock(const fs_shared_ctx_t *ctx)
{
    if (!ctx || !ctx->mutex || !*(ctx->mutex)) return 1;
    return xSemaphoreTake(*(ctx->mutex), pdMS_TO_TICKS(5000)) == pdTRUE;
}

static void ctx_unlock(const fs_shared_ctx_t *ctx)
{
    if (ctx && ctx->mutex && *(ctx->mutex)) xSemaphoreGive(*(ctx->mutex));
}

static int ensure_ctx_ready(const fs_shared_ctx_t *ctx)
{
    if (!ctx) return XST_FAILURE;
    if (fs_shared_is_ready(ctx)) return XST_SUCCESS;
    if (!ctx->root) return XST_FAILURE;
    const fs_device_info_t *dev = fs_device_for_path(ctx->root);
    if (!dev) return XST_FAILURE;
    return fs_device_prepare(dev);
}

static bool normalize_path(const char *in, char *out, size_t out_sz)
{
    char buf[CONSOLE_PATH_MAX * 2];
    if (!in || !out) return false;
    strncpy(buf, in, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    size_t pos = 0;
    char drive[8] = {0};
    if (buf[0] && buf[1] == ':') {
        drive[0] = buf[0];
        drive[1] = ':';
        drive[2] = '\0';
        pos = 2;
        if (buf[pos] == '/') pos++;
    } else if (buf[0] == '/') {
        pos = 1;
    } else {
        return false;
    }

    const char *segs[32];
    int seg_cnt = 0;
    char *p = buf + pos;
    while (p && *p) {
        char *slash = strchr(p, '/');
        if (slash) *slash = '\0';
        if (p[0] == '\0' || strcmp(p, ".") == 0) {
        } else if (strcmp(p, "..") == 0) {
            if (seg_cnt > 0) seg_cnt--;
        } else {
            if (seg_cnt < 32) segs[seg_cnt++] = p;
        }
        if (!slash) break;
        p = slash + 1;
    }

    size_t idx = 0;
    if (drive[0]) idx += snprintf(out + idx, out_sz - idx, "%s/", drive);
    else idx += snprintf(out + idx, out_sz - idx, "/");

    for (int i = 0; i < seg_cnt; ++i) {
        if (idx + strlen(segs[i]) + 2 >= out_sz) return false;
        idx += snprintf(out + idx, out_sz - idx, "%s", segs[i]);
        if (i != seg_cnt - 1) {
            out[idx++] = '/';
            out[idx] = '\0';
        }
    }
    return idx < out_sz;
}

static bool translate_device_path(const char *arg, char *out, size_t out_sz, const fs_device_info_t **out_dev)
{
    if (!arg || !out || out_sz == 0) return false;
    char alias[FS_NAME_MAX] = {0};
    const fs_device_info_t *dev = NULL;
    const char *rest = NULL;

    const char *colon = strchr(arg, ':');
    if (colon && colon > arg) {
        size_t len = (size_t)(colon - arg);
        if (len >= sizeof(alias)) return false;
        memcpy(alias, arg, len);
        alias[len] = '\0';
        dev = fs_device_by_name(alias);
        if (dev) rest = colon + 1;
    }
    if (!dev) {
        const char *slash = strchr(arg, '/');
        size_t len = slash ? (size_t)(slash - arg) : strlen(arg);
        if (len > 0 && len < sizeof(alias)) {
            memcpy(alias, arg, len);
            alias[len] = '\0';
            dev = fs_device_by_name(alias);
            if (dev) rest = slash ? slash : "";
        }
    }
    if (!dev) {
        const fs_device_info_t *exact = fs_device_by_name(arg);
        if (exact) {
            dev = exact;
            rest = "";
        }
    }
    if (!dev || !dev->ctx || !dev->ctx->root) return false;
    if (out_dev) *out_dev = dev;

    const char *root = dev->ctx->root;
    if (!rest || *rest == '\0') {
        strncpy(out, root, out_sz - 1);
        out[out_sz - 1] = '\0';
        return true;
    }
    const char *suffix = rest;
    while (*suffix == '/') suffix++;
    if (*suffix == '\0') {
        strncpy(out, root, out_sz - 1);
        out[out_sz - 1] = '\0';
        return true;
    }

    size_t root_len = strlen(root);
    size_t suffix_len = strlen(suffix);
    if (root_len + suffix_len >= out_sz) return false;
    strncpy(out, root, out_sz - 1);
    out[out_sz - 1] = '\0';
    if (root_len > 0 && out[root_len - 1] == '/') {
        strncat(out, suffix, out_sz - strlen(out) - 1);
    } else {
        if (root_len + 1 + suffix_len >= out_sz) return false;
        out[root_len] = '/';
        out[root_len + 1] = '\0';
        strncat(out, suffix, out_sz - strlen(out) - 1);
    }
    return true;
}

static bool build_path(const console_session_t *session, const char *arg, char *out, size_t out_sz)
{
    const char *root = console_session_get_root(session);
    const char *base = (session && session->cwd[0]) ? session->cwd : root;
    char tmp[CONSOLE_PATH_MAX * 2];
    char alias_resolved[CONSOLE_PATH_MAX * 2];
    const char *input = arg;
    if (translate_device_path(arg, alias_resolved, sizeof(alias_resolved), NULL)) {
        input = alias_resolved;
    }
    if (!input || input[0] == '\0' || strcmp(input, ".") == 0) {
        strncpy(tmp, base, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
    } else if (input[0] == '/' && input[1] == '\0') {
        strncpy(tmp, root, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
    } else if (input[0] == '/') {
        snprintf(tmp, sizeof(tmp), "%s%s", root, input + 1);
    } else if (strchr(input, ':')) {
        strncpy(tmp, input, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
    } else {
        bool need_slash = base[strlen(base) - 1] != '/';
        snprintf(tmp, sizeof(tmp), "%s%s%s", base, need_slash ? "/" : "", input);
    }
    return normalize_path(tmp, out, out_sz);
}

static const fs_shared_ctx_t *resolve_ctx_for_path(const console_session_t *session, const char *path)
{
    const fs_device_info_t *dev = fs_device_for_path(path);
    if (dev && dev->ctx) return dev->ctx;
    return console_session_get_fs(session);
}

static int lock_two(const fs_shared_ctx_t *a, const fs_shared_ctx_t *b)
{
    if (a == b) return ctx_lock(a);
    const fs_shared_ctx_t *first = (a < b) ? a : b;
    const fs_shared_ctx_t *second = (first == a) ? b : a;
    if (!ctx_lock(first)) return 0;
    if (!ctx_lock(second)) { ctx_unlock(first); return 0; }
    return 1;
}

static void unlock_two(const fs_shared_ctx_t *a, const fs_shared_ctx_t *b)
{
    if (a == b) { ctx_unlock(a); return; }
    const fs_shared_ctx_t *first = (a < b) ? a : b;
    const fs_shared_ctx_t *second = (first == a) ? b : a;
    ctx_unlock(second);
    ctx_unlock(first);
}

static int file_write_cb(void *user, const void *buf, size_t len)
{
    FIL *f = (FIL *)user;
    if (!f || !buf) return -1;
    UINT bw = 0;
    if (f_write(f, buf, (UINT)len, &bw) != FR_OK) return -1;
    return (int)bw;
}

static int file_read_cb(void *user, void *buf, size_t len)
{
    FIL *f = (FIL *)user;
    if (!f || !buf) return -1;
    UINT br = 0;
    if (f_read(f, buf, (UINT)len, &br) != FR_OK) return -1;
    return (int)br;
}

static int sock_write_cb(void *user, const void *buf, size_t len)
{
    int fd = *(int *)user;
    int w = lwip_write(fd, buf, (int)len);
    return (w <= 0) ? -1 : w;
}

static void cmd_help_tar(int fd)
{
    write_str(fd, "tar usage:\r\n");
    write_str(fd, "  tar -h|--help\r\n");
    write_str(fd, "  tar c <src_dir> <dst_tar>\r\n");
    write_str(fd, "    create tar file from directory\r\n");
    write_str(fd, "  tar x <src_tar> <dst_dir>\r\n");
    write_str(fd, "    extract tar file into directory\r\n");
    write_str(fd, "  tar t <src_tar>\r\n");
    write_str(fd, "    list tar entries\r\n");
    write_str(fd, "paths:\r\n");
    write_str(fd, "  relative, /abs, 0:/..., 1:/..., sd:/..., flash:/...\r\n");
    write_str(fd, "examples:\r\n");
    write_str(fd, "  tar c logs sd:/backup/logs.tar\r\n");
    write_str(fd, "  tar x sd:/backup/logs.tar /restore\r\n");
}

static void cmd_tar_create(int fd, console_session_t *session, const char *src_arg, const char *dst_arg)
{
    char src_path[CONSOLE_PATH_MAX];
    char dst_path[CONSOLE_PATH_MAX];
    if (!build_path(session, src_arg, src_path, sizeof(src_path)) ||
        !build_path(session, dst_arg, dst_path, sizeof(dst_path))) {
        write_str(fd, "ERR\r\n");
        return;
    }
    const fs_shared_ctx_t *src_ctx = resolve_ctx_for_path(session, src_path);
    const fs_shared_ctx_t *dst_ctx = resolve_ctx_for_path(session, dst_path);
    if (!src_ctx || !dst_ctx) { write_str(fd, "ERR\r\n"); return; }
    if (ensure_ctx_ready(src_ctx) != XST_SUCCESS || ensure_ctx_ready(dst_ctx) != XST_SUCCESS) { write_str(fd, "ERR\r\n"); return; }

    if (fs_shared_fs_is_dir(src_ctx, src_path) != XST_SUCCESS) { write_str(fd, "ERR\r\n"); return; }

    if (!lock_two(src_ctx, dst_ctx)) { write_str(fd, "FS busy\r\n"); return; }

    FIL out;
    if (f_open(&out, dst_path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        unlock_two(src_ctx, dst_ctx);
        write_str(fd, "ERR\r\n");
        return;
    }
    int rc = tar_stream_dir_fatfs(src_ctx, src_path, file_write_cb, &out);
    f_close(&out);
    unlock_two(src_ctx, dst_ctx);
    write_str(fd, (rc == XST_SUCCESS) ? "OK\r\n" : "ERR\r\n");
}

static void cmd_tar_extract(int fd, console_session_t *session, const char *src_arg, const char *dst_arg)
{
    char src_path[CONSOLE_PATH_MAX];
    char dst_path[CONSOLE_PATH_MAX];
    if (!build_path(session, src_arg, src_path, sizeof(src_path)) ||
        !build_path(session, dst_arg, dst_path, sizeof(dst_path))) {
        write_str(fd, "ERR\r\n");
        return;
    }
    const fs_shared_ctx_t *src_ctx = resolve_ctx_for_path(session, src_path);
    const fs_shared_ctx_t *dst_ctx = resolve_ctx_for_path(session, dst_path);
    if (!src_ctx || !dst_ctx) { write_str(fd, "ERR\r\n"); return; }
    if (ensure_ctx_ready(src_ctx) != XST_SUCCESS || ensure_ctx_ready(dst_ctx) != XST_SUCCESS) { write_str(fd, "ERR\r\n"); return; }

    if (!lock_two(src_ctx, dst_ctx)) { write_str(fd, "FS busy\r\n"); return; }

    FIL in;
    if (f_open(&in, src_path, FA_READ) != FR_OK) {
        unlock_two(src_ctx, dst_ctx);
        write_str(fd, "ERR\r\n");
        return;
    }
    int rc = tar_extract_into_dir_fatfs(dst_ctx, dst_path, file_read_cb, &in);
    f_close(&in);
    unlock_two(src_ctx, dst_ctx);
    write_str(fd, (rc == XST_SUCCESS) ? "OK\r\n" : "ERR\r\n");
}

static void cmd_tar_list(int fd, console_session_t *session, const char *src_arg)
{
    char src_path[CONSOLE_PATH_MAX];
    if (!build_path(session, src_arg, src_path, sizeof(src_path))) {
        write_str(fd, "ERR\r\n");
        return;
    }
    const fs_shared_ctx_t *src_ctx = resolve_ctx_for_path(session, src_path);
    if (!src_ctx) { write_str(fd, "ERR\r\n"); return; }
    if (ensure_ctx_ready(src_ctx) != XST_SUCCESS) { write_str(fd, "ERR\r\n"); return; }
    if (!ctx_lock(src_ctx)) { write_str(fd, "FS busy\r\n"); return; }

    FIL in;
    if (f_open(&in, src_path, FA_READ) != FR_OK) {
        ctx_unlock(src_ctx);
        write_str(fd, "ERR\r\n");
        return;
    }
    int u = fd;
    int rc = tar_list(file_read_cb, &in, sock_write_cb, &u);
    f_close(&in);
    ctx_unlock(src_ctx);
    write_str(fd, (rc == XST_SUCCESS) ? "OK\r\n" : "ERR\r\n");
}

bool tar_handle(char *tok, char **save, int fd, console_session_t *session)
{
    if (!tok || strcasecmp(tok, "tar") != 0) return false;
    char *sub = strtok_r(NULL, " \t", save);
    if (!sub || strcasecmp(sub, "-h") == 0 || strcasecmp(sub, "--help") == 0 || strcasecmp(sub, "-help") == 0) {
        cmd_help_tar(fd);
        return true;
    }
    if (strcasecmp(sub, "c") == 0) {
        char *src = strtok_r(NULL, " \t", save);
        char *dst = strtok_r(NULL, " \t", save);
        if (!src || !dst) { write_str(fd, "ERR\r\n"); return true; }
        cmd_tar_create(fd, session, src, dst);
        return true;
    }
    if (strcasecmp(sub, "x") == 0) {
        char *src = strtok_r(NULL, " \t", save);
        char *dst = strtok_r(NULL, " \t", save);
        if (!src || !dst) { write_str(fd, "ERR\r\n"); return true; }
        cmd_tar_extract(fd, session, src, dst);
        return true;
    }
    if (strcasecmp(sub, "t") == 0) {
        char *src = strtok_r(NULL, " \t", save);
        if (!src) { write_str(fd, "ERR\r\n"); return true; }
        cmd_tar_list(fd, session, src);
        return true;
    }
    write_str(fd, "ERR\r\n");
    return true;
}

void tar_help(int fd)
{
    write_str(fd, "  tar -h|--help\r\n");
    write_str(fd, "  tar c <src_dir> <dst_tar>\r\n");
    write_str(fd, "  tar x <src_tar> <dst_dir>\r\n");
    write_str(fd, "  tar t <src_tar>\r\n");
}

