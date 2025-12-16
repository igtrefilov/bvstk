#include "fs_shell.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <strings.h>

#include "lwip/sockets.h"
#include "xstatus.h"
#include "../../fs/fs_devices.h"

#define CONSOLE_PATH_MAX 128

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
    if (drive[0]) {
        idx += snprintf(out + idx, out_sz - idx, "%s/", drive);
    } else {
        idx += snprintf(out + idx, out_sz - idx, "/");
    }
    for (int i = 0; i < seg_cnt; ++i) {
        if (idx + strlen(segs[i]) + 2 >= out_sz) return false;
        idx += snprintf(out + idx, out_sz - idx, "%s", segs[i]);
        if (i != seg_cnt - 1) {
            out[idx++] = '/';
            out[idx] = '\0';
        }
    }
    if (seg_cnt == 0) {
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

static const fs_shared_ctx_t *session_ctx(const console_session_t *session)
{
    return console_session_get_fs(session);
}

static const fs_shared_ctx_t *resolve_fs_ctx_for_path(console_session_t *session, const char *path);

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

static bool shell_basename(const char *path, char *out, size_t out_sz)
{
    if (!path || !out || out_sz == 0) return false;
    const char *end = path + strlen(path);
    while (end > path && *(end - 1) == '/') end--;
    const char *start = end;
    while (start > path && *(start - 1) != '/') start--;
    size_t len = end - start;
    if (len == 0 || len >= out_sz) return false;
    memcpy(out, start, len);
    out[len] = '\0';
    return true;
}

static bool shell_join_path(const char *parent, const char *child, char *out, size_t out_sz)
{
    if (!parent || !child || !out || out_sz == 0) return false;
    size_t parent_len = strlen(parent);
    bool needs_slash = parent_len == 0 || parent[parent_len - 1] != '/';
    size_t child_len = strlen(child);
    size_t needed = parent_len + child_len + (needs_slash ? 1 : 0);
    if (needed >= out_sz) return false;
    if (needs_slash) {
        int wrote = snprintf(out, out_sz, "%s/%s", parent, child);
        return wrote >= 0 && (size_t)wrote < out_sz;
    }
    int wrote = snprintf(out, out_sz, "%s%s", parent, child);
    return wrote >= 0 && (size_t)wrote < out_sz;
}

static void cmd_help_fs(int fd)
{
    write_str(fd, "fs usage:\r\n");
    write_str(fd, "  pwd\r\n");
    write_str(fd, "  ls [path]\r\n");
    write_str(fd, "  cd <dir>\r\n");
    write_str(fd, "  cd flash | cd sd  (switch between flash and sd filesystems)\r\n");
    write_str(fd, "  mkdir <dir>\r\n");
    write_str(fd, "  touch <file>\r\n");
    write_str(fd, "  cat <file>\r\n");
    write_str(fd, "  rm <file|dir>\r\n");
    write_str(fd, "  rm -r <dir>   (recursive)\r\n");
    write_str(fd, "  cp <src> <dst>\r\n");
    write_str(fd, "  cp -r <src> <dst>\r\n");
    write_str(fd, "  mv <src> <dst>\r\n");
    write_str(fd, "  (use sd:/ or flash:/ prefixes to target another device)\r\n");
}

static void cmd_fs_pwd(int fd, console_session_t *session)
{
    const char *cwd = (session && session->cwd[0]) ? session->cwd : console_session_get_root(session);
    char out[CONSOLE_PATH_MAX];
    snprintf(out, sizeof(out), "%s\r\n", cwd);
    (void)lwip_write(fd, out, strlen(out));
}

static void cmd_fs_ls(int fd, console_session_t *session, const char *path)
{
    const fs_shared_ctx_t *ctx = session_ctx(session);
    if (!ctx) { write_str(fd, "ERR\r\n"); return; }
    char full[CONSOLE_PATH_MAX];
    if (!build_path(session, path, full, sizeof(full))) { write_str(fd, "ERR\r\n"); return; }
    if (fs_shared_fs_ls(ctx, full, fd) != XST_SUCCESS) { write_str(fd, "ERR\r\n"); }
}

static void cmd_fs_cd(int fd, console_session_t *session, const char *path)
{
    const fs_device_info_t *alias_dev = NULL;
    char alias_resolved[CONSOLE_PATH_MAX * 2];
    const char *target_path = path;
    if (translate_device_path(path, alias_resolved, sizeof(alias_resolved), &alias_dev)) {
        target_path = alias_resolved;
        if (session && alias_dev && alias_dev->ctx) {
            console_session_set_fs(session, alias_dev->ctx, alias_dev->label);
        }
    }
    const fs_shared_ctx_t *ctx = session_ctx(session);
    if (!ctx) { write_str(fd, "ERR\r\n"); return; }
    char full[CONSOLE_PATH_MAX];
    if (!session || !build_path(session, target_path, full, sizeof(full))) {
        write_str(fd, "ERR\r\n");
        return;
    }
    if (fs_shared_fs_is_dir(ctx, full) != XST_SUCCESS) { write_str(fd, "ERR\r\n"); return; }
    strncpy(session->cwd, full, CONSOLE_CWD_LEN - 1);
    session->cwd[CONSOLE_CWD_LEN - 1] = '\0';
    cmd_fs_pwd(fd, session);
}

static void cmd_fs_mkdir(int fd, console_session_t *session, const char *path)
{
    const fs_shared_ctx_t *ctx = session_ctx(session);
    if (!ctx) { write_str(fd, "ERR\r\n"); return; }
    char full[CONSOLE_PATH_MAX];
    if (!build_path(session, path, full, sizeof(full))) { write_str(fd, "ERR\r\n"); return; }
    FRESULT res = fs_shared_fs_mkdir(ctx, full);
    if (res == FR_OK || res == FR_EXIST) {
        write_str(fd, "OK\r\n");
    } else {
        char buf[32];
        int n = snprintf(buf, sizeof(buf), "ERR (FR=%d)\r\n", res);
        if (n > 0 && n < (int)sizeof(buf)) {
            write_str(fd, buf);
        } else {
            write_str(fd, "ERR\r\n");
        }
    }
}

static void cmd_fs_touch(int fd, console_session_t *session, const char *path)
{
    const fs_shared_ctx_t *ctx = session_ctx(session);
    if (!ctx) { write_str(fd, "ERR\r\n"); return; }
    char full[CONSOLE_PATH_MAX];
    if (!build_path(session, path, full, sizeof(full))) { write_str(fd, "ERR\r\n"); return; }
    FRESULT res = fs_shared_fs_touch(ctx, full);
    if (res == FR_OK) {
        write_str(fd, "OK\r\n");
    } else {
        char buf[32];
        int n = snprintf(buf, sizeof(buf), "ERR (FR=%d)\r\n", res);
        if (n > 0 && n < (int)sizeof(buf)) {
            write_str(fd, buf);
        } else {
            write_str(fd, "ERR\r\n");
        }
    }
}

static void cmd_fs_cat(int fd, console_session_t *session, const char *path)
{
    const fs_shared_ctx_t *ctx = session_ctx(session);
    if (!ctx) { write_str(fd, "ERR\r\n"); return; }
    char full[CONSOLE_PATH_MAX];
    if (!build_path(session, path, full, sizeof(full))) { write_str(fd, "ERR\r\n"); return; }
    if (fs_shared_fs_cat(ctx, full, fd) != XST_SUCCESS) write_str(fd, "ERR\r\n");
}

static void cmd_fs_rm(int fd, console_session_t *session, const char *path, bool recursive)
{
    if (!path) { write_str(fd, "ERR\r\n"); return; }
    char full[CONSOLE_PATH_MAX];
    if (!build_path(session, path, full, sizeof(full))) { write_str(fd, "ERR\r\n"); return; }
    const fs_shared_ctx_t *ctx = resolve_fs_ctx_for_path(session, full);
    if (!ctx) { write_str(fd, "ERR\r\n"); return; }
    FRESULT res = recursive ? fs_shared_fs_rm_recursive(ctx, full) : fs_shared_fs_rm(ctx, full);
    if (res == FR_OK) {
        write_str(fd, "OK\r\n");
    } else {
        char buf[32];
        int n = snprintf(buf, sizeof(buf), "ERR (FR=%d)\r\n", (int)res);
        if (n > 0 && n < (int)sizeof(buf)) write_str(fd, buf);
        else write_str(fd, "ERR\r\n");
    }
}

static const fs_shared_ctx_t *resolve_fs_ctx_for_path(console_session_t *session, const char *path)
{
    if (!path) return NULL;
    const fs_device_info_t *dev = fs_device_for_path(path);
    if (dev) {
        if (fs_device_prepare(dev) != XST_SUCCESS) return NULL;
        return dev->ctx;
    }
    if (path[0] && path[1] == ':') return NULL;
    const fs_shared_ctx_t *ctx = session_ctx(session);
    if (ctx) return ctx;
    const fs_device_info_t *fallback = fs_device_default();
    return fallback ? fallback->ctx : NULL;
}

static void cmd_fs_cp(int fd, console_session_t *session, const char *src_arg, const char *dst_arg, bool recursive)
{
    if (!src_arg || !dst_arg) { write_str(fd, "ERR\r\n"); return; }
    char src_path[CONSOLE_PATH_MAX];
    char dst_path[CONSOLE_PATH_MAX];
    if (!build_path(session, src_arg, src_path, sizeof(src_path)) ||
        !build_path(session, dst_arg, dst_path, sizeof(dst_path))) {
        write_str(fd, "ERR\r\n");
        return;
    }
    const fs_shared_ctx_t *src_ctx = resolve_fs_ctx_for_path(session, src_path);
    const fs_shared_ctx_t *dst_ctx = resolve_fs_ctx_for_path(session, dst_path);
    if (!src_ctx || !dst_ctx) {
        write_str(fd, "ERR\r\n");
        return;
    }
    char resolved_dst[CONSOLE_PATH_MAX];
    strncpy(resolved_dst, dst_path, sizeof(resolved_dst) - 1);
    resolved_dst[sizeof(resolved_dst) - 1] = '\0';
    if (fs_shared_fs_is_dir(dst_ctx, dst_path) == XST_SUCCESS) {
        char base[CONSOLE_PATH_MAX];
        if (!shell_basename(src_path, base, sizeof(base)) ||
            !shell_join_path(dst_path, base, resolved_dst, sizeof(resolved_dst))) {
            write_str(fd, "ERR\r\n");
            return;
        }
    }
    int result;
    if (src_ctx == dst_ctx) {
        result = fs_shared_fs_cp(src_ctx, src_path, resolved_dst, recursive);
    } else {
        result = fs_shared_fs_cp_between(src_ctx, dst_ctx, src_path, resolved_dst, recursive);
    }
    if (result == XST_SUCCESS) {
        write_str(fd, "OK\r\n");
    } else {
        write_str(fd, "ERR\r\n");
    }
}

static void cmd_fs_mv(int fd, console_session_t *session, const char *src_arg, const char *dst_arg)
{
    if (!src_arg || !dst_arg) { write_str(fd, "ERR\r\n"); return; }
    char src_path[CONSOLE_PATH_MAX];
    char dst_path[CONSOLE_PATH_MAX];
    if (!build_path(session, src_arg, src_path, sizeof(src_path)) ||
        !build_path(session, dst_arg, dst_path, sizeof(dst_path))) {
        write_str(fd, "ERR\r\n");
        return;
    }
    const fs_shared_ctx_t *src_ctx = resolve_fs_ctx_for_path(session, src_path);
    const fs_shared_ctx_t *dst_ctx = resolve_fs_ctx_for_path(session, dst_path);
    if (!src_ctx || !dst_ctx) {
        write_str(fd, "ERR\r\n");
        return;
    }
    bool src_is_dir = (fs_shared_fs_is_dir(src_ctx, src_path) == XST_SUCCESS);
    char final_dst[CONSOLE_PATH_MAX];
    strncpy(final_dst, dst_path, sizeof(final_dst) - 1);
    final_dst[sizeof(final_dst) - 1] = '\0';
    if (fs_shared_fs_is_dir(dst_ctx, dst_path) == XST_SUCCESS) {
        char base[CONSOLE_PATH_MAX];
        if (!shell_basename(src_path, base, sizeof(base)) ||
            !shell_join_path(dst_path, base, final_dst, sizeof(final_dst))) {
            write_str(fd, "ERR\r\n");
            return;
        }
    }
    int result;
    if (src_ctx == dst_ctx) {
        result = fs_shared_fs_mv(src_ctx, src_path, final_dst);
    } else {
        result = fs_shared_fs_mv_between(src_ctx, dst_ctx, src_path, final_dst, src_is_dir);
    }
    if (result == XST_SUCCESS) {
        write_str(fd, "OK\r\n");
    } else {
        write_str(fd, "ERR\r\n");
    }
}

bool fs_handle(char *tok, char **save, int fd, console_session_t *session)
{
    if (!tok) return false;
    if (strcasecmp(tok, "fs") == 0) {
        char *sub = strtok_r(NULL, " \t", save);
        if (!sub || strcasecmp(sub, "-h") == 0 || strcasecmp(sub, "--help") == 0 || strcasecmp(sub, "-help") == 0) {
            cmd_help_fs(fd);
        } else {
            write_str(fd, "ERR\r\n");
        }
        return true;
    }
    if (strcasecmp(tok, "pwd") == 0) { cmd_fs_pwd(fd, session); return true; }
    if (strcasecmp(tok, "ls") == 0) { char *p = strtok_r(NULL, " \t", save); cmd_fs_ls(fd, session, p); return true; }
    if (strcasecmp(tok, "cd") == 0) { char *p = strtok_r(NULL, " \t", save); cmd_fs_cd(fd, session, p); return true; }
    if (strcasecmp(tok, "mkdir") == 0) { char *p = strtok_r(NULL, " \t", save); cmd_fs_mkdir(fd, session, p); return true; }
    if (strcasecmp(tok, "touch") == 0) { char *p = strtok_r(NULL, " \t", save); cmd_fs_touch(fd, session, p); return true; }
    if (strcasecmp(tok, "cat") == 0) { char *p = strtok_r(NULL, " \t", save); cmd_fs_cat(fd, session, p); return true; }
    if (strcasecmp(tok, "rm") == 0) {
        char *first = strtok_r(NULL, " \t", save);
        if (!first) { write_str(fd, "ERR\r\n"); return true; }
        bool recursive = false;
        char *path = first;
        if (strcasecmp(first, "-r") == 0 || strcasecmp(first, "-R") == 0 ||
            strcasecmp(first, "-rf") == 0 || strcasecmp(first, "-fr") == 0 ||
            strcasecmp(first, "-Rf") == 0 || strcasecmp(first, "-rF") == 0 ||
            strcasecmp(first, "-RF") == 0 || strcasecmp(first, "-FR") == 0) {
            recursive = true;
            path = strtok_r(NULL, " \t", save);
        }
        cmd_fs_rm(fd, session, path, recursive);
        return true;
    }
    if (strcasecmp(tok, "cp") == 0) {
        char *first = strtok_r(NULL, " \t", save);
        if (!first) { write_str(fd, "ERR\r\n"); return true; }
        bool recursive = false;
        char *src_arg = first;
        if (strcasecmp(first, "-r") == 0 || strcasecmp(first, "-R") == 0) {
            recursive = true;
            src_arg = strtok_r(NULL, " \t", save);
        }
        char *dst_arg = strtok_r(NULL, " \t", save);
        if (!src_arg || !dst_arg) { write_str(fd, "ERR\r\n"); return true; }
        cmd_fs_cp(fd, session, src_arg, dst_arg, recursive);
        return true;
    }
    if (strcasecmp(tok, "mv") == 0) {
        char *src = strtok_r(NULL, " \t", save);
        char *dst = strtok_r(NULL, " \t", save);
        if (!src || !dst) { write_str(fd, "ERR\r\n"); return true; }
        cmd_fs_mv(fd, session, src, dst);
        return true;
    }
    return false;
}

void fs_help(int fd)
{
    write_str(fd, "  pwd\r\n");
    write_str(fd, "  ls [path]\r\n");
    write_str(fd, "  cd <dir>\r\n");
    write_str(fd, "  cd flash | cd sd  (switch devices)\r\n");
    write_str(fd, "  mkdir <dir>\r\n");
    write_str(fd, "  touch <file>\r\n");
    write_str(fd, "  cat <file>\r\n");
    write_str(fd, "  rm <file|dir>\r\n");
    write_str(fd, "  rm -r <dir>\r\n");
    write_str(fd, "  cp <src> <dst>\r\n");
    write_str(fd, "  cp -r <src> <dst>\r\n");
    write_str(fd, "  mv <src> <dst>\r\n");
    write_str(fd, "  (use sd:/ or flash:/ prefixes to target another device)\r\n");
}
