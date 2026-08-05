#include "apps/freertos/services/ssh/bvstk_ssh_server.h"

#ifdef BVSTK_SSH_ENABLE

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "FreeRTOS.h"
#include "task.h"
#include "xil_printf.h"
#include "xstatus.h"

#include "lwip/sockets.h"
#include "lwip/sys.h"

#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/hash.h>
#include <wolfssl/wolfcrypt/random.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssh/ssh.h>
#ifdef WOLFSSH_SCP
#include <wolfssh/wolfscp.h>
#endif

#include "apps/freertos/config/config_store.h"
#include "apps/freertos/console/console_common.h"
#include "apps/freertos/console/console_stream.h"
#include "apps/freertos/console/utils.h"
#include "apps/freertos/services/ssh/bvstk_ssh_generated.h"

#ifndef SSH_THREAD_STACKSIZE
#define SSH_THREAD_STACKSIZE 12288
#endif

#define SSH_CONSOLE_FD (-1)
#define SSH_LINE_SIZE 256
#define SSH_RX_SIZE 1024
#define SSH_MATCH_MAX 32
#define SSH_TOKEN_MAX 4
#define SSH_HISTORY_LEN 16

#ifdef WOLFSSH_SCP
#define SCP_PATH_SIZE FS_PATH_MAX
#define SCP_MAX_DEPTH 16

typedef struct {
    const fs_device_info_t *recv_device;
    int recv_requested;
    int recv_recursive;
    char recv_base[SCP_PATH_SIZE];
    char recv_dir_paths[SCP_MAX_DEPTH][SCP_PATH_SIZE];
    size_t recv_depth;
    int recv_base_is_dir;
    FIL recv_file;
    int recv_file_open;

    const fs_device_info_t *send_device;
    int send_requested;
    int send_recursive;
    char send_path[SCP_PATH_SIZE];
    DIR send_dirs[SCP_MAX_DEPTH];
    char send_dir_paths[SCP_MAX_DEPTH][SCP_PATH_SIZE];
    size_t send_depth;
    FIL send_file;
    int send_file_open;
    uint32_t send_file_size;
    uint32_t send_read_bytes;
} bvstk_scp_session_t;
#endif

enum {
    SSH_ESC_NONE = 0,
    SSH_ESC_ESC,
    SSH_ESC_CSI,
    SSH_ESC_SS3
};

typedef struct {
    WOLFSSH *ssh;
    WOLFSSH_CHANNEL *channel;
    word32 channel_id;
    console_session_t console;
    char line[SSH_LINE_SIZE];
    size_t line_len;
    size_t cursor;
    char history[SSH_HISTORY_LEN][SSH_LINE_SIZE];
    size_t history_count;
    int history_pos;
    char history_scratch[SSH_LINE_SIZE];
    size_t history_scratch_len;
    char cut_buffer[SSH_LINE_SIZE];
    size_t cut_len;
    int escape_state;
    char escape_params[16];
    size_t escape_param_len;
    int shell_requested;
    int exec_requested;
    int exec_done;
    int banner_sent;
    int close_requested;
    int last_was_cr;
    char exec_command[SSH_LINE_SIZE];
#ifdef WOLFSSH_SCP
    bvstk_scp_session_t scp;
#endif
} bvstk_ssh_session_t;

static WOLFSSH_CTX *s_ssh_ctx;

static const char *const s_ssh_commands[] = {
    "fs", "tar", "ip", "smi", "spi", "mem", "i2c",
    "pwd", "ls", "cd", "mkdir", "touch", "cat", "rm", "cp", "mv",
    "help", "reboot", "quit", "exit"
};

typedef struct {
    const char *items[SSH_MATCH_MAX];
    char storage[SSH_MATCH_MAX][I2C_CFG_NAME_MAX * 2];
    size_t count;
} ssh_match_set_t;

extern int bvstk_ssh_seed(OS_Seed *os, byte *output, word32 size);

static int ssh_user_auth(byte auth_type, WS_UserAuthData *auth_data, void *ctx)
{
    (void)ctx;
    if (!auth_data || auth_type != WOLFSSH_USERAUTH_PASSWORD) {
        return WOLFSSH_USERAUTH_FAILURE;
    }
    if (auth_data->usernameSz != (word32)strlen(BVSTK_SSH_USER) ||
        memcmp(auth_data->username, BVSTK_SSH_USER, auth_data->usernameSz) != 0) {
        return WOLFSSH_USERAUTH_FAILURE;
    }

    byte digest[WC_SHA256_DIGEST_SIZE];
    if (wc_Sha256Hash(auth_data->sf.password.password,
                      auth_data->sf.password.passwordSz, digest) != 0) {
        return WOLFSSH_USERAUTH_FAILURE;
    }
    return (sizeof(digest) == BVSTK_SSH_PASSWORD_SHA256_SIZE &&
            memcmp(digest, bvstk_ssh_password_sha256, sizeof(digest)) == 0) ?
           WOLFSSH_USERAUTH_SUCCESS : WOLFSSH_USERAUTH_FAILURE;
}

#ifdef WOLFSSH_SCP
static int scp_path_has_parent_component(const char *path)
{
    const char *p = path;
    while (p && *p) {
        const char *start;
        size_t len;

        while (*p == '/') ++p;
        if (*p == '\0') break;
        start = p;
        while (*p && *p != '/') {
            if (*p == '\\') return 1;
            ++p;
        }
        len = (size_t)(p - start);
        if ((len == 1 && start[0] == '.') ||
            (len == 2 && start[0] == '.' && start[1] == '.')) {
            return 1;
        }
    }
    return 0;
}

static int scp_resolve_path(const char *input, const fs_device_info_t **device_out,
                            char *path_out, size_t path_out_size)
{
    const char *path = input;
    const fs_device_info_t *device = NULL;
    const char *relative = NULL;
    int needed;

    if (!input || !device_out || !path_out || path_out_size == 0) return 0;
    while (*path == ' ' || *path == '\t') ++path;

    /* scp clients and wolfSSH may present an absolute-looking alias as
     * /sd:/file. FatFs aliases are kept as sd:/file internally. */
    if (path[0] == '/' &&
        (!strncasecmp(path + 1, "sd:", 3) ||
         !strncasecmp(path + 1, "flash:", 6) ||
         !strncasecmp(path + 1, "0:", 2) ||
         !strncasecmp(path + 1, "1:", 2))) {
        ++path;
    }

    if (!strncasecmp(path, "sd:", 3)) {
        device = fs_device_by_name("sd");
        relative = path + 3;
        if (*relative == '/') ++relative;
    } else if (!strncasecmp(path, "flash:", 6)) {
        device = fs_device_by_name("flash");
        relative = path + 6;
        if (*relative == '/') ++relative;
    } else if (!strncasecmp(path, "0:", 2)) {
        device = fs_device_by_name("sd");
        relative = path + 2;
        if (*relative == '/') ++relative;
    } else if (!strncasecmp(path, "1:", 2)) {
        device = fs_device_by_name("flash");
        relative = path + 2;
        if (*relative == '/') ++relative;
    }

    if (!device || !device->ctx || !device->ctx->root ||
        scp_path_has_parent_component(path)) return 0;

    if (relative) {
        needed = snprintf(path_out, path_out_size, "%s%s",
                          device->ctx->root, relative);
    } else {
        needed = snprintf(path_out, path_out_size, "%s", path);
    }
    if (needed < 0 || (size_t)needed >= path_out_size) return 0;
    *device_out = device;
    return 1;
}

static int scp_join_file_path(const char *base, const char *file_name,
                              char *path_out, size_t path_out_size)
{
    size_t base_len;
    int needed;

    if (!base || !file_name || !file_name[0] || !path_out || path_out_size == 0 ||
        strchr(file_name, '/') || strchr(file_name, '\\') ||
        !strcmp(file_name, ".") || !strcmp(file_name, "..")) return 0;
    base_len = strlen(base);
    needed = snprintf(path_out, path_out_size, "%s%s%s", base,
                      (base_len && base[base_len - 1] == '/') ? "" : "/",
                      file_name);
    return needed >= 0 && (size_t)needed < path_out_size;
}

static int scp_copy_basename(const char *path, char *name, size_t name_size)
{
    const char *end;
    const char *start;
    size_t length;

    if (!path || !name || name_size == 0) return 0;
    end = path + strlen(path);
    while (end > path && end[-1] == '/') --end;
    start = end;
    while (start > path && start[-1] != '/') --start;
    length = (size_t)(end - start);
    if (length == 0 || length >= name_size) return 0;
    memcpy(name, start, length);
    name[length] = '\0';
    return 1;
}

static int scp_copy_entry_name(const FILINFO *info, char *name, size_t name_size)
{
    const char *source;

    if (!info || !name || name_size == 0) return 0;
#if FF_USE_LFN
    source = info->fname[0] ? info->fname : info->altname;
#else
    source = info->fname;
#endif
    if (!source || !source[0] || !strcmp(source, ".") || !strcmp(source, "..")) {
        return 0;
    }
    if (strchr(source, '/') || strchr(source, '\\')) return 0;
    if (strlen(source) >= name_size) return 0;
    strcpy(name, source);
    return 1;
}

static int scp_fail(WOLFSSH *ssh, const char *message);

static int scp_make_directory(WOLFSSH *ssh, const fs_device_info_t *device,
                              const char *path)
{
    FRESULT res;
    FILINFO info;

    res = fs_shared_fs_mkdir(device->ctx, path);
    if (res == FR_OK) return 1;
    if (res != FR_EXIST) {
        (void)scp_fail(ssh, "cannot create destination directory");
        return 0;
    }
    res = fs_shared_file_stat(device->ctx, path, &info);
    if (res != FR_OK || !(info.fattrib & AM_DIR)) {
        (void)scp_fail(ssh, "destination path is not a directory");
        return 0;
    }
    return 1;
}

static int scp_fail(WOLFSSH *ssh, const char *message)
{
    if (ssh && message) (void)wolfSSH_SetScpErrorMsg(ssh, message);
    return WS_SCP_ABORT;
}

static int scp_validate_receive_base(WOLFSSH *ssh, const char *base_path,
                                     bvstk_scp_session_t *scp)
{
    const fs_device_info_t *device = NULL;
    FILINFO info;
    char resolved[SCP_PATH_SIZE];
    FRESULT res;

    if (!scp_resolve_path(base_path, &device, resolved, sizeof(resolved))) {
        return scp_fail(ssh, "invalid destination path");
    }
    if (fs_device_prepare(device) != XST_SUCCESS) {
        return scp_fail(ssh, "destination filesystem is not ready");
    }

    /* Some xilffs versions reject f_stat() on a volume root even though
     * f_opendir() accepts it.  FatFs context roots are trusted directory
     * handles, so handle that case explicitly. */
    if (!strcmp(resolved, device->ctx->root)) {
        scp->recv_base_is_dir = 1;
    } else {
        res = fs_shared_file_stat(device->ctx, resolved, &info);
        if (res == FR_OK) {
            scp->recv_base_is_dir = (info.fattrib & AM_DIR) != 0;
        } else if (res == FR_NO_FILE || res == FR_NO_PATH) {
            char parent[SCP_PATH_SIZE];
            char *slash = strrchr(resolved, '/');
            if (!slash) return scp_fail(ssh, "invalid destination path");
            size_t parent_len = (size_t)(slash - resolved) + 1;
            if (parent_len >= sizeof(parent)) return scp_fail(ssh, "destination path too long");
            memcpy(parent, resolved, parent_len);
            parent[parent_len] = '\0';
            if (!strcmp(parent, device->ctx->root)) {
                res = FR_OK;
                info.fattrib = AM_DIR;
            } else {
                res = fs_shared_file_stat(device->ctx, parent, &info);
            }
            if (res != FR_OK || !(info.fattrib & AM_DIR)) {
                return scp_fail(ssh, "destination directory not found");
            }
            scp->recv_base_is_dir = 0;
        } else {
            return scp_fail(ssh, "cannot inspect destination path");
        }
    }

    scp->recv_device = device;
    strncpy(scp->recv_base, resolved, sizeof(scp->recv_base) - 1);
    scp->recv_base[sizeof(scp->recv_base) - 1] = '\0';
    scp->recv_recursive = 0;
    scp->recv_depth = 1;
    strncpy(scp->recv_dir_paths[0], scp->recv_base,
            sizeof(scp->recv_dir_paths[0]) - 1);
    scp->recv_dir_paths[0][sizeof(scp->recv_dir_paths[0]) - 1] = '\0';
    return WS_SCP_CONTINUE;
}

static int scp_recv_callback(WOLFSSH *ssh, int state, const char *base_path,
                             const char *file_name, int file_mode, word64 mtime,
                             word64 atime, word32 total_file_size, byte *buf,
                             word32 buf_size, word32 file_offset, void *ctx)
{
    bvstk_scp_session_t *scp = (bvstk_scp_session_t *)ctx;
    char target[SCP_PATH_SIZE];
    uint32_t written = 0;
    FRESULT res;

    (void)file_mode;
    (void)mtime;
    (void)atime;
    (void)total_file_size;
    (void)file_offset;

    if (!ssh || !scp) return WS_SCP_ABORT;

    switch (state) {
        case WOLFSSH_SCP_NEW_REQUEST:
            scp->recv_requested = 1;
            return scp_validate_receive_base(ssh, base_path, scp);

        case WOLFSSH_SCP_NEW_FILE:
            if (!scp->recv_device || scp->recv_file_open || !file_name) {
                return scp_fail(ssh, "invalid incoming file state");
            }
            if (scp->recv_recursive) {
                if (scp->recv_depth == 0 ||
                    !scp_join_file_path(scp->recv_dir_paths[scp->recv_depth - 1],
                                        file_name, target, sizeof(target))) {
                    return scp_fail(ssh, "invalid incoming file name");
                }
            } else if (scp->recv_base_is_dir) {
                if (!scp_join_file_path(scp->recv_base, file_name,
                                        target, sizeof(target))) {
                    return scp_fail(ssh, "invalid incoming file name");
                }
            } else {
                strncpy(target, scp->recv_base, sizeof(target) - 1);
                target[sizeof(target) - 1] = '\0';
            }
            res = fs_shared_file_open_write(scp->recv_device->ctx, target,
                                            &scp->recv_file);
            if (res != FR_OK) return scp_fail(ssh, "cannot open destination file");
            scp->recv_file_open = 1;
            return WS_SCP_CONTINUE;

        case WOLFSSH_SCP_FILE_PART:
            if (!scp->recv_file_open || (buf_size != 0 && !buf)) {
                return scp_fail(ssh, "invalid incoming file data");
            }
            res = fs_shared_file_write(scp->recv_device->ctx, &scp->recv_file,
                                       buf, buf_size, &written);
            if (res != FR_OK || written != buf_size) {
                (void)fs_shared_file_close(scp->recv_device->ctx, &scp->recv_file);
                scp->recv_file_open = 0;
                return scp_fail(ssh, "cannot write destination file");
            }
            return WS_SCP_CONTINUE;

        case WOLFSSH_SCP_FILE_DONE:
            if (!scp->recv_file_open) return scp_fail(ssh, "destination file is not open");
            res = fs_shared_file_close(scp->recv_device->ctx, &scp->recv_file);
            scp->recv_file_open = 0;
            return (res == FR_OK) ? WS_SCP_CONTINUE :
                   scp_fail(ssh, "cannot close destination file");

        case WOLFSSH_SCP_NEW_DIR:
            if (!scp->recv_device || !scp->recv_base_is_dir ||
                !file_name || scp->recv_depth == 0 ||
                scp->recv_depth >= SCP_MAX_DEPTH) {
                return scp_fail(ssh, "invalid incoming directory state");
            }
            if (!scp_join_file_path(scp->recv_dir_paths[scp->recv_depth - 1],
                                    file_name, target, sizeof(target))) {
                return scp_fail(ssh, "invalid incoming directory name");
            }
            if (!scp_make_directory(ssh, scp->recv_device, target)) return WS_SCP_ABORT;
            strncpy(scp->recv_dir_paths[scp->recv_depth], target,
                    sizeof(scp->recv_dir_paths[scp->recv_depth]) - 1);
            scp->recv_dir_paths[scp->recv_depth][sizeof(scp->recv_dir_paths[scp->recv_depth]) - 1] = '\0';
            ++scp->recv_depth;
            scp->recv_recursive = 1;
            return WS_SCP_CONTINUE;

        case WOLFSSH_SCP_END_DIR:
            if (!scp->recv_recursive || scp->recv_depth <= 1) {
                return scp_fail(ssh, "invalid incoming directory end");
            }
            --scp->recv_depth;
            return WS_SCP_CONTINUE;

        default:
            return scp_fail(ssh, "invalid SCP receive state");
    }
}

static void scp_send_close_dirs(bvstk_scp_session_t *scp)
{
    if (!scp || !scp->send_device) return;
    while (scp->send_depth > 0) {
        (void)fs_shared_dir_close(scp->send_device->ctx,
                                  &scp->send_dirs[scp->send_depth - 1]);
        --scp->send_depth;
    }
    scp->send_recursive = 0;
}

static int scp_send_recursive_init(WOLFSSH *ssh, const char *peer_request,
                                   char *file_name, word32 file_name_size,
                                   word64 *mtime, word64 *atime, int *file_mode,
                                   bvstk_scp_session_t *scp)
{
    const fs_device_info_t *device = NULL;
    FILINFO info;
    char resolved[SCP_PATH_SIZE];
    FRESULT res;

    if (!peer_request || !file_name || !mtime || !atime || !file_mode || !scp ||
        !scp_resolve_path(peer_request, &device, resolved, sizeof(resolved))) {
        return scp_fail(ssh, "invalid recursive source path");
    }
    if (fs_device_prepare(device) != XST_SUCCESS) {
        return scp_fail(ssh, "source filesystem is not ready");
    }
    if (!strcmp(resolved, device->ctx->root)) {
        return scp_fail(ssh, "copying filesystem root is not supported");
    }
    res = fs_shared_file_stat(device->ctx, resolved, &info);
    if (res != FR_OK || !(info.fattrib & AM_DIR)) {
        return scp_fail(ssh, "source directory not found");
    }
    if (!scp_copy_basename(resolved, file_name, file_name_size)) {
        return scp_fail(ssh, "source directory name is invalid");
    }
    if (strlen(resolved) >= sizeof(scp->send_dir_paths[0])) {
        return scp_fail(ssh, "source directory path is too long");
    }

    if (scp->send_file_open && scp->send_device) {
        (void)fs_shared_file_close(scp->send_device->ctx, &scp->send_file);
        scp->send_file_open = 0;
    }
    scp_send_close_dirs(scp);
    res = fs_shared_dir_open(device->ctx, resolved, &scp->send_dirs[0]);
    if (res != FR_OK) return scp_fail(ssh, "cannot open source directory");
    scp->send_device = device;
    scp->send_depth = 1;
    strncpy(scp->send_dir_paths[0], resolved,
            sizeof(scp->send_dir_paths[0]) - 1);
    scp->send_dir_paths[0][sizeof(scp->send_dir_paths[0]) - 1] = '\0';
    scp->send_recursive = 1;
    *mtime = 0;
    *atime = 0;
    *file_mode = 0755;
    return WS_SCP_ENTER_DIR;
}

static int scp_send_recursive_next(WOLFSSH *ssh, char *file_name,
                                    word32 file_name_size, word64 *mtime,
                                    word64 *atime, int *file_mode,
                                    word32 *total_file_size, byte *buf,
                                    word32 buf_size, bvstk_scp_session_t *scp)
{
    FILINFO info;
    char entry_name[FS_NAME_MAX];
    char child_path[SCP_PATH_SIZE];
    FRESULT res;

    if (!scp || !scp->send_device || !scp->send_recursive ||
        scp->send_depth == 0 || !file_name || !mtime || !atime || !file_mode ||
        !total_file_size) {
        return scp_fail(ssh, "invalid recursive source state");
    }

    for (;;) {
        size_t top = scp->send_depth - 1;
        res = fs_shared_dir_read(scp->send_device->ctx, &scp->send_dirs[top], &info);
        if (res != FR_OK) return scp_fail(ssh, "cannot read source directory");
        if (info.fname[0] == '\0') {
            (void)fs_shared_dir_close(scp->send_device->ctx, &scp->send_dirs[top]);
            --scp->send_depth;
            if (scp->send_depth == 0) {
                scp->send_recursive = 0;
                return WS_SCP_EXIT_DIR_FINAL;
            }
            return WS_SCP_EXIT_DIR;
        }
        if (!scp_copy_entry_name(&info, entry_name, sizeof(entry_name))) {
            continue;
        }
        if (!scp_join_file_path(scp->send_dir_paths[top], entry_name,
                                child_path, sizeof(child_path))) {
            return scp_fail(ssh, "source path is too long");
        }

        if (info.fattrib & AM_DIR) {
            if (scp->send_depth >= SCP_MAX_DEPTH) {
                return scp_fail(ssh, "source directory nesting is too deep");
            }
            res = fs_shared_dir_open(scp->send_device->ctx, child_path,
                                     &scp->send_dirs[scp->send_depth]);
            if (res != FR_OK) return scp_fail(ssh, "cannot open nested directory");
            strncpy(scp->send_dir_paths[scp->send_depth], child_path,
                    sizeof(scp->send_dir_paths[scp->send_depth]) - 1);
            scp->send_dir_paths[scp->send_depth][sizeof(scp->send_dir_paths[scp->send_depth]) - 1] = '\0';
            ++scp->send_depth;
            if (strlen(entry_name) >= file_name_size) {
                return scp_fail(ssh, "source entry name is too long");
            }
            strcpy(file_name, entry_name);
            *mtime = 0;
            *atime = 0;
            *file_mode = 0755;
            *total_file_size = 0;
            return WS_SCP_ENTER_DIR;
        }

        res = fs_shared_file_open_read(scp->send_device->ctx, child_path,
                                       &scp->send_file, &scp->send_file_size);
        if (res != FR_OK) return scp_fail(ssh, "cannot open source file");
        scp->send_file_open = 1;
        strncpy(scp->send_path, child_path, sizeof(scp->send_path) - 1);
        scp->send_path[sizeof(scp->send_path) - 1] = '\0';
        if (strlen(entry_name) >= file_name_size) {
            (void)fs_shared_file_close(scp->send_device->ctx, &scp->send_file);
            scp->send_file_open = 0;
            return scp_fail(ssh, "source entry name is too long");
        }
        strcpy(file_name, entry_name);
        *mtime = 0;
        *atime = 0;
        *file_mode = 0644;
        *total_file_size = scp->send_file_size;
        if (scp->send_file_size == 0) {
            (void)fs_shared_file_close(scp->send_device->ctx, &scp->send_file);
            scp->send_file_open = 0;
            return 0;
        }
        res = fs_shared_file_read(scp->send_device->ctx, &scp->send_file,
                                  buf, buf_size, &scp->send_read_bytes);
        if (res != FR_OK || scp->send_read_bytes == 0) {
            (void)fs_shared_file_close(scp->send_device->ctx, &scp->send_file);
            scp->send_file_open = 0;
            return scp_fail(ssh, "cannot read source file");
        }
        return scp->send_read_bytes;
    }
}

static int scp_send_callback(WOLFSSH *ssh, int state, const char *peer_request,
                             char *file_name, word32 file_name_size, word64 *mtime,
                             word64 *atime, int *file_mode, word32 file_offset,
                             word32 *total_file_size, byte *buf, word32 buf_size,
                             void *ctx)
{
    bvstk_scp_session_t *scp = (bvstk_scp_session_t *)ctx;
    const fs_device_info_t *device = NULL;
    FILINFO info;
    FRESULT res;
    uint32_t read_bytes = 0;

    if (!ssh || !scp) return WS_SCP_ABORT;

    switch (state) {
        case WOLFSSH_SCP_NEW_REQUEST:
            scp->send_requested = 1;
            return WS_SCP_CONTINUE;

        case WOLFSSH_SCP_SINGLE_FILE_REQUEST:
            if (!peer_request || !file_name || !mtime || !atime || !file_mode ||
                !total_file_size || (!buf && buf_size != 0)) {
                return scp_fail(ssh, "invalid SCP source request");
            }
            if (scp->send_file_open) {
                (void)fs_shared_file_close(scp->send_device->ctx, &scp->send_file);
                scp->send_file_open = 0;
            }
            if (!scp_resolve_path(peer_request, &device, scp->send_path,
                                  sizeof(scp->send_path))) {
                return scp_fail(ssh, "invalid source path");
            }
            if (fs_device_prepare(device) != XST_SUCCESS) {
                return scp_fail(ssh, "source filesystem is not ready");
            }
            res = fs_shared_file_stat(device->ctx, scp->send_path, &info);
            if (res != FR_OK || (info.fattrib & AM_DIR)) {
                return scp_fail(ssh, "source file not found");
            }
            res = fs_shared_file_open_read(device->ctx, scp->send_path,
                                           &scp->send_file, &scp->send_file_size);
            if (res != FR_OK) return scp_fail(ssh, "cannot open source file");
            scp->send_device = device;
            scp->send_file_open = 1;
            if (!scp_copy_basename(scp->send_path, file_name, file_name_size)) {
                (void)fs_shared_file_close(device->ctx, &scp->send_file);
                scp->send_file_open = 0;
                return scp_fail(ssh, "source file name is too long");
            }
            *mtime = 0;
            *atime = 0;
            *file_mode = 0644;
            *total_file_size = scp->send_file_size;
            break;

        case WOLFSSH_SCP_RECURSIVE_REQUEST:
            if (!scp->send_recursive) {
                return scp_send_recursive_init(ssh, peer_request, file_name,
                                               file_name_size, mtime, atime,
                                               file_mode, scp);
            }
            return scp_send_recursive_next(ssh, file_name, file_name_size,
                                           mtime, atime, file_mode,
                                           total_file_size, buf, buf_size, scp);

        case WOLFSSH_SCP_CONTINUE_FILE_TRANSFER:
            if (!scp->send_file_open || (!buf && buf_size != 0) || !total_file_size) {
                return scp_fail(ssh, "invalid SCP source state");
            }
            break;

        default:
            return scp_fail(ssh, "invalid SCP send state");
    }

    res = fs_shared_file_read(scp->send_device->ctx, &scp->send_file,
                              buf, buf_size, &read_bytes);
    if (res != FR_OK || (read_bytes == 0 && file_offset < scp->send_file_size)) {
        (void)fs_shared_file_close(scp->send_device->ctx, &scp->send_file);
        scp->send_file_open = 0;
        return scp_fail(ssh, "cannot read source file");
    }
    if (file_offset + read_bytes >= scp->send_file_size) {
        (void)fs_shared_file_close(scp->send_device->ctx, &scp->send_file);
        scp->send_file_open = 0;
    }
    return (int)read_bytes;
}

static void scp_cleanup(bvstk_scp_session_t *scp)
{
    if (!scp) return;
    if (scp->recv_file_open && scp->recv_device) {
        (void)fs_shared_file_close(scp->recv_device->ctx, &scp->recv_file);
        scp->recv_file_open = 0;
    }
    if (scp->send_file_open && scp->send_device) {
        (void)fs_shared_file_close(scp->send_device->ctx, &scp->send_file);
        scp->send_file_open = 0;
    }
    scp_send_close_dirs(scp);
}
#endif

static int ssh_channel_store(bvstk_ssh_session_t *session,
                              WOLFSSH_CHANNEL *channel)
{
    if (!session || !channel) return WS_BAD_ARGUMENT;
    session->channel = channel;
    if (wolfSSH_ChannelGetId(channel, &session->channel_id,
                             WS_CHANNEL_ID_SELF) != WS_SUCCESS) {
        return WS_INVALID_CHANID;
    }
    return WS_SUCCESS;
}

static int ssh_shell_request(WOLFSSH_CHANNEL *channel, void *ctx)
{
    bvstk_ssh_session_t *session = (bvstk_ssh_session_t *)ctx;
    int ret = ssh_channel_store(session, channel);
    if (ret == WS_SUCCESS) session->shell_requested = 1;
    return ret;
}

static int ssh_exec_request(WOLFSSH_CHANNEL *channel, void *ctx)
{
    bvstk_ssh_session_t *session = (bvstk_ssh_session_t *)ctx;
    int ret = ssh_channel_store(session, channel);
    const char *command = wolfSSH_ChannelGetSessionCommand(channel);
    if (ret != WS_SUCCESS || !command) return WS_BAD_ARGUMENT;
    strncpy(session->exec_command, command, sizeof(session->exec_command) - 1);
    session->exec_command[sizeof(session->exec_command) - 1] = '\0';
    session->exec_requested = 1;
    return WS_SUCCESS;
}

static int ssh_write(void *ctx, const void *data, size_t len)
{
    bvstk_ssh_session_t *session = (bvstk_ssh_session_t *)ctx;
    if (!session || !session->channel || (!data && len != 0)) return -1;
    const byte *ptr = (const byte *)data;
    size_t sent = 0;
    while (sent < len) {
        word32 chunk = (word32)((len - sent > 1024) ? 1024 : (len - sent));
        int ret = wolfSSH_ChannelSend(session->channel, ptr + sent, chunk);
        if (ret <= 0) return -1;
        sent += (size_t)ret;
    }
    return (int)sent;
}

static void ssh_send_text(bvstk_ssh_session_t *session, const char *text)
{
    if (session && text) (void)ssh_write(session, text, strlen(text));
}

static void ssh_move_cursor(bvstk_ssh_session_t *session, size_t position)
{
    if (!session) return;
    if (position > session->line_len) position = session->line_len;
    while (session->cursor > position) {
        ssh_send_text(session, "\x1b[D");
        --session->cursor;
    }
    while (session->cursor < position) {
        ssh_send_text(session, "\x1b[C");
        ++session->cursor;
    }
}

static void ssh_redraw_line(bvstk_ssh_session_t *session)
{
    if (!session) return;
    size_t desired_cursor = session->cursor;
    ssh_send_text(session, "\r\x1b[2K");
    console_print_prompt(SSH_CONSOLE_FD, &session->console);
    if (session->line_len != 0) {
        (void)ssh_write(session, session->line, session->line_len);
    }
    session->cursor = session->line_len;
    ssh_move_cursor(session, desired_cursor);
}

static void ssh_history_add(bvstk_ssh_session_t *session)
{
    if (!session || session->line_len == 0) return;
    if (session->history_count > 0 &&
        strcmp(session->history[session->history_count - 1], session->line) == 0) {
        return;
    }
    if (session->history_count < SSH_HISTORY_LEN) {
        memcpy(session->history[session->history_count], session->line,
               session->line_len + 1);
        ++session->history_count;
    } else {
        for (size_t i = 1; i < SSH_HISTORY_LEN; ++i) {
            memcpy(session->history[i - 1], session->history[i], SSH_LINE_SIZE);
        }
        memcpy(session->history[SSH_HISTORY_LEN - 1], session->line,
               session->line_len + 1);
    }
}

static void ssh_load_line(bvstk_ssh_session_t *session,
                           const char *line, size_t line_len)
{
    if (!session || !line) return;
    if (line_len >= sizeof(session->line)) line_len = sizeof(session->line) - 1;
    memcpy(session->line, line, line_len);
    session->line[line_len] = '\0';
    session->line_len = line_len;
    session->cursor = line_len;
    ssh_redraw_line(session);
}

static void ssh_history_up(bvstk_ssh_session_t *session)
{
    if (!session || session->history_count == 0) return;
    if (session->history_pos < 0) {
        memcpy(session->history_scratch, session->line, session->line_len + 1);
        session->history_scratch_len = session->line_len;
        session->history_pos = 0;
    } else if ((size_t)(session->history_pos + 1) < session->history_count) {
        ++session->history_pos;
    } else {
        return;
    }
    size_t index = session->history_count - 1 - (size_t)session->history_pos;
    ssh_load_line(session, session->history[index], strlen(session->history[index]));
}

static void ssh_history_down(bvstk_ssh_session_t *session)
{
    if (!session || session->history_pos < 0) return;
    if (session->history_pos > 0) {
        --session->history_pos;
        size_t index = session->history_count - 1 - (size_t)session->history_pos;
        ssh_load_line(session, session->history[index], strlen(session->history[index]));
    } else {
        session->history_pos = -1;
        ssh_load_line(session, session->history_scratch,
                      session->history_scratch_len);
    }
}

static void ssh_cut_range(bvstk_ssh_session_t *session, size_t start, size_t end)
{
    if (!session || start >= end || end > session->line_len) return;
    session->cut_len = end - start;
    if (session->cut_len >= sizeof(session->cut_buffer)) {
        session->cut_len = sizeof(session->cut_buffer) - 1;
    }
    memcpy(session->cut_buffer, session->line + start, session->cut_len);
    session->cut_buffer[session->cut_len] = '\0';
    memmove(session->line + start, session->line + end,
            session->line_len - end);
    session->line_len -= end - start;
    session->line[session->line_len] = '\0';
    session->cursor = start;
    session->history_pos = -1;
    ssh_redraw_line(session);
}

static void ssh_cut_previous_word(bvstk_ssh_session_t *session)
{
    if (!session || session->cursor == 0) return;
    size_t start = session->cursor;
    while (start > 0 && isspace((unsigned char)session->line[start - 1])) --start;
    while (start > 0 && !isspace((unsigned char)session->line[start - 1])) --start;
    ssh_cut_range(session, start, session->cursor);
}

static void ssh_delete_before_cursor(bvstk_ssh_session_t *session)
{
    if (!session || session->cursor == 0) return;
    memmove(session->line + session->cursor - 1,
            session->line + session->cursor,
            session->line_len - session->cursor);
    --session->cursor;
    --session->line_len;
    session->line[session->line_len] = '\0';
    session->history_pos = -1;
    ssh_redraw_line(session);
}

static void ssh_delete_at_cursor(bvstk_ssh_session_t *session)
{
    if (!session || session->cursor >= session->line_len) return;
    memmove(session->line + session->cursor,
            session->line + session->cursor + 1,
            session->line_len - session->cursor - 1);
    --session->line_len;
    session->line[session->line_len] = '\0';
    session->history_pos = -1;
    ssh_redraw_line(session);
}

static void ssh_insert_cut_buffer(bvstk_ssh_session_t *session)
{
    if (!session || session->cut_len == 0 ||
        session->line_len + session->cut_len >= sizeof(session->line)) return;
    memmove(session->line + session->cursor + session->cut_len,
            session->line + session->cursor,
            session->line_len - session->cursor);
    memcpy(session->line + session->cursor, session->cut_buffer, session->cut_len);
    session->line_len += session->cut_len;
    session->cursor += session->cut_len;
    session->line[session->line_len] = '\0';
    session->history_pos = -1;
    ssh_redraw_line(session);
}

static void ssh_move_word_left(bvstk_ssh_session_t *session)
{
    if (!session) return;
    size_t position = session->cursor;
    while (position > 0 && isspace((unsigned char)session->line[position - 1])) --position;
    while (position > 0 && !isspace((unsigned char)session->line[position - 1])) --position;
    ssh_move_cursor(session, position);
}

static void ssh_move_word_right(bvstk_ssh_session_t *session)
{
    if (!session) return;
    size_t position = session->cursor;
    while (position < session->line_len &&
           isspace((unsigned char)session->line[position])) ++position;
    while (position < session->line_len &&
           !isspace((unsigned char)session->line[position])) ++position;
    ssh_move_cursor(session, position);
}

static size_t ssh_common_prefix_ci(const char *const *matches, size_t count)
{
    if (!matches || count == 0 || !matches[0]) return 0;
    size_t length = strlen(matches[0]);
    for (size_t i = 1; i < count; ++i) {
        size_t common = 0;
        while (common < length && matches[i][common] &&
               tolower((unsigned char)matches[0][common]) ==
               tolower((unsigned char)matches[i][common])) {
            ++common;
        }
        length = common;
        if (length == 0) break;
    }
    return length;
}

static void ssh_match_set_init(ssh_match_set_t *set)
{
    if (!set) return;
    memset(set, 0, sizeof(*set));
}

static void ssh_match_add(ssh_match_set_t *set, const char *prefix,
                           const char *candidate)
{
    if (!set || !candidate) return;
    if (!prefix) prefix = "";
    size_t prefix_len = strlen(prefix);
    if (strncasecmp(candidate, prefix, prefix_len) != 0 ||
        set->count >= SSH_MATCH_MAX) return;
    strncpy(set->storage[set->count], candidate,
            sizeof(set->storage[set->count]) - 1);
    set->storage[set->count][sizeof(set->storage[set->count]) - 1] = '\0';
    set->items[set->count] = set->storage[set->count];
    ++set->count;
}

static void ssh_match_add_words(ssh_match_set_t *set, const char *prefix,
                                const char *const *words, size_t words_count)
{
    if (!set || !words) return;
    for (size_t i = 0; i < words_count; ++i) {
        ssh_match_add(set, prefix, words[i]);
    }
}

static void ssh_collect_i2c_selector(const char *prefix, ssh_match_set_t *set)
{
    static const char *const head_words[] = { "list", "-h", "--help" };
    ssh_match_add_words(set, prefix, head_words,
                        sizeof(head_words) / sizeof(head_words[0]));
    if (!config_store_is_ready()) return;

    const i2c_device_config_t *devices = config_store_get_i2c_devices();
    size_t count = config_store_get_i2c_device_count();
    if (!devices || count == 0) return;
    if (prefix && prefix[0] == '@') {
        for (size_t i = 0; i < count; ++i) {
            char address[I2C_CFG_NAME_MAX * 2];
            snprintf(address, sizeof(address), "@0x%02X",
                     (unsigned)(devices[i].addr_7b & 0x7Fu));
            ssh_match_add(set, prefix, address);
        }
    } else {
        for (size_t i = 0; i < count; ++i) {
            if (devices[i].name[0]) ssh_match_add(set, prefix, devices[i].name);
        }
    }
}

static void ssh_collect_smi_selector(const char *prefix, ssh_match_set_t *set)
{
    static const char *const head_words[] = { "list", "r", "w", "-h", "--help" };
    ssh_match_add_words(set, prefix, head_words,
                        sizeof(head_words) / sizeof(head_words[0]));
    if (!config_store_is_ready()) return;

    const smi_phy_config_t *devices = config_store_get_smi_devices();
    size_t count = config_store_get_smi_device_count();
    if (!devices || count == 0) return;
    if (prefix && prefix[0] == '@') {
        for (size_t i = 0; i < count; ++i) {
            char address[I2C_CFG_NAME_MAX * 2];
            snprintf(address, sizeof(address), "@%u",
                     (unsigned)(devices[i].phy_addr & 0x1Fu));
            ssh_match_add(set, prefix, address);
        }
    } else {
        for (size_t i = 0; i < count; ++i) {
            if (devices[i].name[0]) ssh_match_add(set, prefix, devices[i].name);
        }
    }
}

static size_t ssh_split_tokens_before(const char *line, size_t upto,
                                      char tokens[][I2C_CFG_NAME_MAX * 2],
                                      size_t tokens_max)
{
    size_t count = 0;
    size_t position = 0;
    while (position < upto) {
        while (position < upto && (line[position] == ' ' || line[position] == '\t')) {
            ++position;
        }
        if (position >= upto) break;
        size_t start = position;
        while (position < upto && line[position] != ' ' && line[position] != '\t') {
            ++position;
        }
        if (count < tokens_max) {
            size_t length = position - start;
            if (length >= sizeof(tokens[count])) length = sizeof(tokens[count]) - 1;
            memcpy(tokens[count], line + start, length);
            tokens[count][length] = '\0';
            ++count;
        }
    }
    return count;
}

static void ssh_collect_argument_matches(const char *const *tokens,
                                         size_t token_count,
                                         const char *prefix,
                                         ssh_match_set_t *set)
{
    if (!tokens || token_count == 0) return;
    const char *command = tokens[0];
    if (strcasecmp(command, "i2c") == 0) {
        if (token_count == 1) {
            ssh_collect_i2c_selector(prefix, set);
        } else if (token_count == 2 && strcasecmp(tokens[1], "list") != 0) {
            static const char *const words[] = {
                "info", "r", "w", "addr", "address", "policy", "autopoll"
            };
            ssh_match_add_words(set, prefix, words, sizeof(words) / sizeof(words[0]));
        } else if (token_count >= 3 && strcasecmp(tokens[2], "policy") == 0 &&
                   strcasecmp(tokens[1], "list") != 0) {
            if (token_count == 3) {
                static const char *const words[] = {
                    "show", "set", "whitelist", "blacklist"
                };
                ssh_match_add_words(set, prefix, words, sizeof(words) / sizeof(words[0]));
            } else if (token_count == 4 && strcasecmp(tokens[3], "show") == 0) {
                static const char *const words[] = { "rules", "whitelist", "blacklist" };
                ssh_match_add_words(set, prefix, words, sizeof(words) / sizeof(words[0]));
            } else if (token_count == 4 && strcasecmp(tokens[3], "set") == 0) {
                static const char *const words[] = { "whitelist", "blacklist" };
                ssh_match_add_words(set, prefix, words, sizeof(words) / sizeof(words[0]));
            } else if (token_count == 4 &&
                       (strcasecmp(tokens[3], "whitelist") == 0 ||
                        strcasecmp(tokens[3], "blacklist") == 0)) {
                static const char *const words[] = { "add", "del", "delete", "clear" };
                ssh_match_add_words(set, prefix, words, sizeof(words) / sizeof(words[0]));
            }
        }
    } else if (strcasecmp(command, "smi") == 0) {
        if (token_count == 1) {
            ssh_collect_smi_selector(prefix, set);
        } else if (token_count == 2 && strcasecmp(tokens[1], "list") != 0 &&
                   strcasecmp(tokens[1], "r") != 0 && strcasecmp(tokens[1], "w") != 0) {
            static const char *const words[] = {
                "info", "r", "w", "phy", "addr", "address", "rules", "policy",
                "allow", "deny", "clear", "autopoll", "settings", "save"
            };
            ssh_match_add_words(set, prefix, words, sizeof(words) / sizeof(words[0]));
        } else if (token_count == 3 && strcasecmp(tokens[2], "policy") == 0) {
            static const char *const words[] = { "whitelist", "blacklist" };
            ssh_match_add_words(set, prefix, words, sizeof(words) / sizeof(words[0]));
        } else if (token_count == 3 && strcasecmp(tokens[2], "autopoll") == 0) {
            static const char *const words[] = {
                "on", "off", "reg_delay", "cycle_delay", "regs"
            };
            ssh_match_add_words(set, prefix, words, sizeof(words) / sizeof(words[0]));
        } else if (token_count == 3 && strcasecmp(tokens[2], "settings") == 0) {
            static const char *const words[] = { "clear" };
            ssh_match_add_words(set, prefix, words, sizeof(words) / sizeof(words[0]));
        }
    }
}

static void ssh_apply_matches(bvstk_ssh_session_t *session, size_t prefix_len,
                              const ssh_match_set_t *set)
{
    if (!session || !set || set->count == 0) return;
    if (set->count == 1) {
        const char *match = set->items[0];
        size_t match_len = strlen(match);
        size_t suffix_len = (match_len > prefix_len) ? match_len - prefix_len : 0;
        size_t add_space = (session->cursor == session->line_len) ? 1 : 0;
        if (session->line_len + suffix_len + add_space >= sizeof(session->line)) return;
        size_t tail = session->line_len - session->cursor;
        if (suffix_len != 0) {
            memmove(session->line + session->cursor + suffix_len,
                    session->line + session->cursor, tail);
            memcpy(session->line + session->cursor, match + prefix_len, suffix_len);
            session->line_len += suffix_len;
            session->cursor += suffix_len;
        }
        if (add_space) {
            session->line[session->line_len++] = ' ';
            ++session->cursor;
        }
        session->line[session->line_len] = '\0';
        session->history_pos = -1;
        ssh_redraw_line(session);
        return;
    }

    size_t common_len = ssh_common_prefix_ci(set->items, set->count);
    if (common_len > prefix_len && common_len < sizeof(session->line) - 1) {
        size_t suffix_len = common_len - prefix_len;
        size_t tail = session->line_len - session->cursor;
        if (session->line_len + suffix_len >= sizeof(session->line)) return;
        memmove(session->line + session->cursor + suffix_len,
                session->line + session->cursor, tail);
        memcpy(session->line + session->cursor,
               set->items[0] + prefix_len, suffix_len);
        session->line_len += suffix_len;
        session->cursor += suffix_len;
        session->history_pos = -1;
    }

    ssh_send_text(session, "\r\n");
    for (size_t i = 0; i < set->count; ++i) {
        ssh_send_text(session, set->items[i]);
        ssh_send_text(session, "  ");
    }
    ssh_send_text(session, "\r\n");
    ssh_redraw_line(session);
}

static void ssh_complete_command(bvstk_ssh_session_t *session)
{
    if (!session || session->cursor >= sizeof(session->line)) return;

    size_t start = session->cursor;
    while (start > 0 && session->line[start - 1] != ' ' &&
           session->line[start - 1] != '\t') {
        --start;
    }
    size_t prefix_len = session->cursor - start;
    char token_storage[SSH_TOKEN_MAX][I2C_CFG_NAME_MAX * 2];
    const char *tokens[SSH_TOKEN_MAX];
    size_t token_count = ssh_split_tokens_before(session->line, start,
                                                 token_storage, SSH_TOKEN_MAX);
    for (size_t i = 0; i < token_count; ++i) tokens[i] = token_storage[i];

    ssh_match_set_t matches;
    ssh_match_set_init(&matches);
    if (token_count == 0) {
        ssh_match_add_words(&matches, session->line + start,
                            s_ssh_commands,
                            sizeof(s_ssh_commands) / sizeof(s_ssh_commands[0]));
    } else {
        ssh_collect_argument_matches(tokens, token_count,
                                      session->line + start, &matches);
    }
    ssh_apply_matches(session, prefix_len, &matches);
}

static void ssh_handle_escape_final(bvstk_ssh_session_t *session, byte c)
{
    if (!session) return;
    int ctrl = (strchr(session->escape_params, '5') != NULL);
    switch (c) {
        case 'A':
            ssh_history_up(session);
            break;
        case 'B':
            ssh_history_down(session);
            break;
        case 'C':
            if (ctrl) {
                ssh_move_word_right(session);
            } else if (session->cursor < session->line_len) {
                ssh_move_cursor(session, session->cursor + 1);
            }
            break;
        case 'D':
            if (ctrl) {
                ssh_move_word_left(session);
            } else if (session->cursor > 0) {
                ssh_move_cursor(session, session->cursor - 1);
            }
            break;
        case 'H':
            ssh_move_cursor(session, 0);
            break;
        case 'F':
            ssh_move_cursor(session, session->line_len);
            break;
        case '~':
            if (session->escape_params[0] == '1' ||
                session->escape_params[0] == '7') {
                ssh_move_cursor(session, 0);
            } else if (session->escape_params[0] == '4' ||
                       session->escape_params[0] == '8') {
                ssh_move_cursor(session, session->line_len);
            } else if (session->escape_params[0] == '3') {
                ssh_delete_at_cursor(session);
            }
            break;
        default:
            break;
    }
}

static void ssh_process_escape_byte(bvstk_ssh_session_t *session, byte c)
{
    if (!session) return;
    if (session->escape_state == SSH_ESC_ESC) {
        if (c == '[') {
            session->escape_state = SSH_ESC_CSI;
            session->escape_param_len = 0;
            session->escape_params[0] = '\0';
        } else if (c == 'O') {
            session->escape_state = SSH_ESC_SS3;
        } else {
            session->escape_state = SSH_ESC_NONE;
        }
        return;
    }
    if (session->escape_state == SSH_ESC_CSI) {
        if ((c >= '0' && c <= '9') || c == ';') {
            if (session->escape_param_len + 1 < sizeof(session->escape_params)) {
                session->escape_params[session->escape_param_len++] = (char)c;
                session->escape_params[session->escape_param_len] = '\0';
            }
            return;
        }
        ssh_handle_escape_final(session, c);
        session->escape_state = SSH_ESC_NONE;
        session->escape_param_len = 0;
        return;
    }
    if (session->escape_state == SSH_ESC_SS3) {
        ssh_handle_escape_final(session, c);
        session->escape_state = SSH_ESC_NONE;
    }
}

static void ssh_process_line(bvstk_ssh_session_t *session)
{
    if (!session) return;
    session->line[session->line_len] = '\0';
    if (session->line_len != 0) {
        ssh_history_add(session);
        process_console_line(session->line, SSH_CONSOLE_FD, &session->console);
    }
    session->line_len = 0;
    session->cursor = 0;
    session->history_pos = -1;
    if (utils_should_close()) session->close_requested = 1;
}

static void ssh_process_input(bvstk_ssh_session_t *session,
                              const byte *data, size_t len)
{
    if (!session || !data) return;
    for (size_t i = 0; i < len && !session->close_requested; ++i) {
        byte c = data[i];
        if (session->escape_state != SSH_ESC_NONE) {
            ssh_process_escape_byte(session, c);
            continue;
        }
        if (c == 0x1b) {
            session->escape_state = SSH_ESC_ESC;
            session->escape_param_len = 0;
            session->escape_params[0] = '\0';
            continue;
        }
        if (c == '\r' || c == '\n') {
            if (c == '\n' && session->last_was_cr) {
                session->last_was_cr = 0;
                continue;
            }
            ssh_send_text(session, "\r\n");
            ssh_process_line(session);
            session->last_was_cr = (c == '\r');
            if (!session->close_requested) console_print_prompt(SSH_CONSOLE_FD, &session->console);
        } else if (c == '\b' || c == 0x7f) {
            session->last_was_cr = 0;
            ssh_delete_before_cursor(session);
        } else if (c == '\t') {
            session->last_was_cr = 0;
            ssh_complete_command(session);
        } else if (c == 0x01) { /* Ctrl-A */
            session->last_was_cr = 0;
            ssh_move_cursor(session, 0);
        } else if (c == 0x02) { /* Ctrl-B */
            session->last_was_cr = 0;
            if (session->cursor > 0) ssh_move_cursor(session, session->cursor - 1);
        } else if (c == 0x03) { /* Ctrl-C */
            session->last_was_cr = 0;
            session->line_len = 0;
            session->cursor = 0;
            session->history_pos = -1;
            ssh_send_text(session, "^C\r\n");
            console_print_prompt(SSH_CONSOLE_FD, &session->console);
        } else if (c == 0x04) { /* Ctrl-D */
            session->last_was_cr = 0;
            ssh_delete_at_cursor(session);
        } else if (c == 0x05) { /* Ctrl-E */
            session->last_was_cr = 0;
            ssh_move_cursor(session, session->line_len);
        } else if (c == 0x06) { /* Ctrl-F */
            session->last_was_cr = 0;
            if (session->cursor < session->line_len) ssh_move_cursor(session, session->cursor + 1);
        } else if (c == 0x0b) { /* Ctrl-K */
            session->last_was_cr = 0;
            ssh_cut_range(session, session->cursor, session->line_len);
        } else if (c == 0x0e) { /* Ctrl-N */
            session->last_was_cr = 0;
            ssh_history_down(session);
        } else if (c == 0x10) { /* Ctrl-P */
            session->last_was_cr = 0;
            ssh_history_up(session);
        } else if (c == 0x15) { /* Ctrl-U */
            session->last_was_cr = 0;
            ssh_cut_range(session, 0, session->cursor);
        } else if (c == 0x17) { /* Ctrl-W */
            session->last_was_cr = 0;
            ssh_cut_previous_word(session);
        } else if (c == 0x19) { /* Ctrl-Y */
            session->last_was_cr = 0;
            ssh_insert_cut_buffer(session);
        } else if (c >= 0x20 && c != 0x7f) {
            session->last_was_cr = 0;
            if (session->line_len + 1 < sizeof(session->line)) {
                size_t tail = session->line_len - session->cursor;
                memmove(session->line + session->cursor + 1,
                        session->line + session->cursor, tail);
                session->line[session->cursor++] = (char)c;
                ++session->line_len;
                session->line[session->line_len] = '\0';
                session->history_pos = -1;
                ssh_redraw_line(session);
            }
        }
    }
}

static void ssh_finish_session(bvstk_ssh_session_t *session, int status)
{
    if (!session || !session->ssh) return;
    (void)wolfSSH_stream_exit(session->ssh, status);
}

static void ssh_service_client(int fd)
{
    bvstk_ssh_session_t session;
    memset(&session, 0, sizeof(session));
    session.history_pos = -1;
    console_session_init(&session.console);
    utils_reset_close();

    WOLFSSH *ssh = wolfSSH_new(s_ssh_ctx);
    if (!ssh) {
        xil_printf("SSH: client context allocation failed, free heap=%u\r\n",
                   (unsigned)xPortGetFreeHeapSize());
        return;
    }
    session.ssh = ssh;
    wolfSSH_set_fd(ssh, fd);
    wolfSSH_SetUserAuthCtx(ssh, NULL);
    wolfSSH_SetChannelReqCtx(ssh, &session);
#ifdef WOLFSSH_SCP
    wolfSSH_SetScpRecvCtx(ssh, &session.scp);
    wolfSSH_SetScpSendCtx(ssh, &session.scp);
#endif

    xil_printf("SSH: accepting client\r\n");
    int accept_ret = wolfSSH_accept(ssh);
#ifdef WOLFSSH_SCP
    int scp_requested = (accept_ret == WS_SCP_INIT);
    if (accept_ret != WS_SUCCESS && accept_ret != WS_SCP_INIT &&
        accept_ret != WS_SCP_COMPLETE) {
#else
    if (accept_ret != WS_SUCCESS) {
#endif
        xil_printf("SSH: accept failed ret=%d err=%d\r\n",
                   accept_ret, wolfSSH_get_error(ssh));
        wolfSSH_free(ssh);
        return;
    }

    /* wolfSSH_worker() is a non-blocking state machine.  Keep the SSH
     * handshake blocking, then let the worker flush exec output and observe
     * the peer close without waiting forever for another input packet. */
    unsigned long nonblocking = 1;
    if (lwip_ioctl(fd, FIONBIO, &nonblocking) != 0) {
        xil_printf("SSH: cannot enable nonblocking client socket\r\n");
        wolfSSH_free(ssh);
        return;
    }

    if (console_stream_register(SSH_CONSOLE_FD, ssh_write, &session) != 0) {
        wolfSSH_free(ssh);
        return;
    }

#ifdef WOLFSSH_SCP
    if (scp_requested || accept_ret == WS_SCP_COMPLETE) {
        int scp_ret = accept_ret;
        int scp_error = wolfSSH_get_error(ssh);

        while (scp_ret != WS_SCP_COMPLETE) {
            if (scp_ret != WS_SCP_INIT && scp_ret != WS_WANT_READ &&
                scp_ret != WS_WANT_WRITE &&
                scp_ret != WS_REKEYING && scp_ret != WS_WINDOW_FULL &&
                scp_ret != WS_CHAN_RXD && scp_error != WS_WANT_READ &&
                scp_error != WS_WANT_WRITE && scp_error != WS_REKEYING &&
                scp_error != WS_WINDOW_FULL && scp_error != WS_CHAN_RXD) {
                xil_printf("SSH: SCP failed ret=%d err=%d\r\n", scp_ret, scp_error);
                break;
            }
            scp_ret = wolfSSH_accept(ssh);
            scp_error = wolfSSH_get_error(ssh);
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        if (scp_ret == WS_SCP_COMPLETE) {
            xil_printf("SSH: SCP transfer completed\r\n");
            /* The wolfSSH sink acknowledges the final file but does not
             * emit the channel exit status required by OpenSSH scp. */
            if (session.scp.recv_requested) {
                (void)wolfSSH_stream_exit(ssh, 0);
            }
        }
        console_stream_unregister(SSH_CONSOLE_FD);
        scp_cleanup(&session.scp);
        wolfSSH_free(ssh);
        return;
    }
#endif

    for (;;) {
        word32 last_channel = 0;
        int ret = wolfSSH_worker(ssh, &last_channel);
        int error = wolfSSH_get_error(ssh);

        if (session.exec_requested && !session.exec_done) {
            session.exec_done = 1;
            process_console_line(session.exec_command, SSH_CONSOLE_FD, &session.console);
            ssh_finish_session(&session, utils_should_close() ? 1 : 0);
        }

        /* wolfSSH_ChannelSend() queues the command output before the exit
         * packets. Give wolfSSH_worker() a chance to flush that output. */
        if (session.exec_done) {
            if (ret == WS_EOF || error == WS_EOF || ret == WS_CHANNEL_CLOSED ||
                error == WS_CHANNEL_CLOSED) {
                break;
            }
            if (ret < 0 && error != WS_WANT_READ && error != WS_WANT_WRITE &&
                error != WS_REKEYING) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        if (session.shell_requested && !session.banner_sent) {
            session.banner_sent = 1;
            console_print_banner(SSH_CONSOLE_FD);
            console_print_prompt(SSH_CONSOLE_FD, &session.console);
        }

        if (ret == WS_CHAN_RXD || error == WS_CHAN_RXD) {
            if (session.channel && last_channel == session.channel_id) {
                byte rx[SSH_RX_SIZE];
                int count = wolfSSH_ChannelIdRead(ssh, session.channel_id,
                                                  rx, sizeof(rx));
                if (count <= 0) break;
                ssh_process_input(&session, rx, (size_t)count);
                if (session.close_requested) {
                    ssh_finish_session(&session, 0);
                    break;
                }
            }
            continue;
        }

        if (ret < 0 && error != WS_WANT_READ && error != WS_WANT_WRITE &&
            error != WS_REKEYING) {
            break;
        }
        if (ret == WS_EOF || error == WS_EOF || ret == WS_CHANNEL_CLOSED ||
            error == WS_CHANNEL_CLOSED) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    console_stream_unregister(SSH_CONSOLE_FD);
#ifdef WOLFSSH_SCP
    scp_cleanup(&session.scp);
#endif
    wolfSSH_free(ssh);
}

static void ssh_server_thread(void *arg)
{
    (void)arg;
    int listen_fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        xil_printf("SSH: socket failed\r\n");
        vTaskDelete(NULL);
        return;
    }

    int reuse = 1;
    (void)lwip_setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(BVSTK_SSH_PORT);
    address.sin_addr.s_addr = INADDR_ANY;
    if (lwip_bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        lwip_listen(listen_fd, 1) < 0) {
        xil_printf("SSH: bind/listen failed\r\n");
        lwip_close(listen_fd);
        vTaskDelete(NULL);
        return;
    }

    xil_printf("SSH: listening on port %u\r\n", (unsigned)BVSTK_SSH_PORT);
    for (;;) {
        struct sockaddr_in remote;
        socklen_t remote_len = sizeof(remote);
        int client_fd = lwip_accept(listen_fd, (struct sockaddr *)&remote, &remote_len);
        if (client_fd < 0) continue;
        xil_printf("SSH: client connected fd=%d\r\n", client_fd);
        ssh_service_client(client_fd);
        xil_printf("SSH: client closed\r\n");
        lwip_close(client_fd);
    }
}

void start_ssh_server(void)
{
    if (wolfSSH_Init() != WS_SUCCESS) {
        xil_printf("SSH: library init failed\r\n");
        return;
    }
    /* wolfSSH_Init() installs wolfCrypt's default seed callback.  Override it
     * afterwards because this FreeRTOS image has no /dev/urandom. */
    if (wc_SetSeed_Cb(bvstk_ssh_seed) != 0) {
        xil_printf("SSH: RNG callback setup failed\r\n");
        return;
    }
    s_ssh_ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, NULL);
    if (!s_ssh_ctx) {
        xil_printf("SSH: context allocation failed\r\n");
        return;
    }
    if (wolfSSH_CTX_UsePrivateKey_buffer(s_ssh_ctx, bvstk_ssh_host_key,
                                         BVSTK_SSH_HOST_KEY_SIZE,
                                         WOLFSSH_FORMAT_PEM) != WS_SUCCESS) {
        xil_printf("SSH: host key rejected\r\n");
        wolfSSH_CTX_free(s_ssh_ctx);
        s_ssh_ctx = NULL;
        return;
    }
    if (wolfSSH_CTX_SetAlgoListKex(s_ssh_ctx,
                                   "ecdh-sha2-nistp256") != WS_SUCCESS) {
        xil_printf("SSH: KEX configuration failed\r\n");
        wolfSSH_CTX_free(s_ssh_ctx);
        s_ssh_ctx = NULL;
        return;
    }
    wolfSSH_SetUserAuth(s_ssh_ctx, ssh_user_auth);
    wolfSSH_CTX_SetChannelReqShellCb(s_ssh_ctx, ssh_shell_request);
    wolfSSH_CTX_SetChannelReqExecCb(s_ssh_ctx, ssh_exec_request);
#ifdef WOLFSSH_SCP
    wolfSSH_SetScpRecv(s_ssh_ctx, scp_recv_callback);
    wolfSSH_SetScpSend(s_ssh_ctx, scp_send_callback);
#endif
    sys_thread_new("ssh_server_thrd", ssh_server_thread, NULL,
                   SSH_THREAD_STACKSIZE, tskIDLE_PRIORITY + 1);
}

#else

void start_ssh_server(void)
{
}

#endif
