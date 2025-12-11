#include "fs_shell.h"

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

static const fs_shared_ctx_t *session_ctx(const console_session_t *session)
{
    return console_session_get_fs(session);
}

static bool build_path(const console_session_t *session, const char *arg, char *out, size_t out_sz)
{
    const char *root = console_session_get_root(session);
    const char *base = (session && session->cwd[0]) ? session->cwd : root;
    char tmp[CONSOLE_PATH_MAX * 2];
    if (!arg || arg[0] == '\0' || strcmp(arg, ".") == 0) {
        strncpy(tmp, base, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
    } else if (arg[0] == '/' && arg[1] == '\0') {
        strncpy(tmp, root, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
    } else if (arg[0] == '/') {
        snprintf(tmp, sizeof(tmp), "%s%s", root, arg + 1);
    } else if (strchr(arg, ':')) {
        strncpy(tmp, arg, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
    } else {
        bool need_slash = base[strlen(base) - 1] != '/';
        snprintf(tmp, sizeof(tmp), "%s%s%s", base, need_slash ? "/" : "", arg);
    }
    return normalize_path(tmp, out, out_sz);
}

static void cmd_help_fs(int fd)
{
    write_str(fd, "fs usage:\r\n");
    write_str(fd, "  pwd\r\n");
    write_str(fd, "  ls [path]\r\n");
    write_str(fd, "  cd <dir>\r\n");
    write_str(fd, "  mkdir <dir>\r\n");
    write_str(fd, "  touch <file>\r\n");
    write_str(fd, "  cat <file>\r\n");
    write_str(fd, "  rm <file|dir>\r\n");
    write_str(fd, "  use <device>\r\n");
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
    const fs_shared_ctx_t *ctx = session_ctx(session);
    if (!ctx) { write_str(fd, "ERR\r\n"); return; }
    char full[CONSOLE_PATH_MAX];
    if (!session || !build_path(session, path, full, sizeof(full))) { write_str(fd, "ERR\r\n"); return; }
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
    if (fs_shared_fs_mkdir(ctx, full) == XST_SUCCESS) write_str(fd, "OK\r\n"); else write_str(fd, "ERR\r\n");
}

static void cmd_fs_touch(int fd, console_session_t *session, const char *path)
{
    const fs_shared_ctx_t *ctx = session_ctx(session);
    if (!ctx) { write_str(fd, "ERR\r\n"); return; }
    char full[CONSOLE_PATH_MAX];
    if (!build_path(session, path, full, sizeof(full))) { write_str(fd, "ERR\r\n"); return; }
    if (fs_shared_fs_touch(ctx, full) == XST_SUCCESS) write_str(fd, "OK\r\n"); else write_str(fd, "ERR\r\n");
}

static void cmd_fs_cat(int fd, console_session_t *session, const char *path)
{
    const fs_shared_ctx_t *ctx = session_ctx(session);
    if (!ctx) { write_str(fd, "ERR\r\n"); return; }
    char full[CONSOLE_PATH_MAX];
    if (!build_path(session, path, full, sizeof(full))) { write_str(fd, "ERR\r\n"); return; }
    if (fs_shared_fs_cat(ctx, full, fd) != XST_SUCCESS) write_str(fd, "ERR\r\n");
}

static void cmd_fs_rm(int fd, console_session_t *session, const char *path)
{
    const fs_shared_ctx_t *ctx = session_ctx(session);
    if (!ctx) { write_str(fd, "ERR\r\n"); return; }
    char full[CONSOLE_PATH_MAX];
    if (!build_path(session, path, full, sizeof(full))) { write_str(fd, "ERR\r\n"); return; }
    if (fs_shared_fs_rm(ctx, full) == XST_SUCCESS) write_str(fd, "OK\r\n"); else write_str(fd, "ERR\r\n");
}

static void cmd_fs_use(int fd, console_session_t *session, const char *name)
{
    if (!session || !name) { write_str(fd, "ERR\r\n"); return; }
    const fs_device_info_t *dev = fs_device_by_name(name);
    if (!dev || !dev->ctx) { write_str(fd, "ERR\r\n"); return; }
    if (fs_device_prepare(dev) != XST_SUCCESS) { write_str(fd, "ERR\r\n"); return; }
    console_session_set_fs(session, dev->ctx, dev->label);
    write_str(fd, "OK\r\n");
}

bool fs_handle(char *tok, char **save, int fd, console_session_t *session)
{
    if (!tok) return false;
    if (strcasecmp(tok, "fs") == 0) {
        char *sub = strtok_r(NULL, " \t", save);
        if (!sub || strcasecmp(sub, "-h") == 0 || strcasecmp(sub, "--help") == 0 || strcasecmp(sub, "-help") == 0) {
            cmd_help_fs(fd);
        } else if (strcasecmp(sub, "use") == 0) {
            char *dev = strtok_r(NULL, " \t", save);
            cmd_fs_use(fd, session, dev);
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
    if (strcasecmp(tok, "rm") == 0) { char *p = strtok_r(NULL, " \t", save); cmd_fs_rm(fd, session, p); return true; }
    return false;
}

void fs_help(int fd)
{
    write_str(fd, "  pwd\r\n");
    write_str(fd, "  ls [path]\r\n");
    write_str(fd, "  cd <dir>\r\n");
    write_str(fd, "  mkdir <dir>\r\n");
    write_str(fd, "  touch <file>\r\n");
    write_str(fd, "  cat <file>\r\n");
    write_str(fd, "  rm <file|dir>\r\n");
    write_str(fd, "  fs use <device>\r\n");
}
