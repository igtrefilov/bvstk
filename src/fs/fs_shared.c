#include "fs_shared.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "lwip/sockets.h"
#include "xil_printf.h"

#include "../bvstk_tcp_server/utils/utils.h"

static int fs_shared_lock(const fs_shared_ctx_t *ctx)
{
    if (!ctx || !ctx->mutex || !*(ctx->mutex)) return 0;
    return xSemaphoreTake(*(ctx->mutex), pdMS_TO_TICKS(200)) == pdTRUE;
}

static void fs_shared_unlock(const fs_shared_ctx_t *ctx)
{
    if (ctx && ctx->mutex && *(ctx->mutex)) {
        xSemaphoreGive(*(ctx->mutex));
    }
}

static int fs_shared_ensure_ready(const fs_shared_ctx_t *ctx, int fd)
{
    if (!ctx || !ctx->ready || !*(ctx->ready)) {
        write_str(fd, "FS not ready\r\n");
        return 0;
    }
    return 1;
}

static void normalize_name(char *s)
{
    if (!s) return;
    bool has_lower = false, has_upper = false;
    for (char *p = s; *p; ++p) {
        if (islower((unsigned char)*p)) has_lower = true;
        if (isupper((unsigned char)*p)) has_upper = true;
    }
    if (has_upper && !has_lower) {
        for (char *p = s; *p; ++p) {
            *p = (char)tolower((unsigned char)*p);
        }
    }
}

int fs_shared_mount(fs_shared_ctx_t *ctx, const char *label)
{
    if (!ctx || !ctx->fatfs || !ctx->ready || !ctx->root) return XST_FAILURE;
    FRESULT res = f_mount(ctx->fatfs, ctx->root, 1);
    if (res != FR_OK) {
        BYTE work[FF_MAX_SS];
        res = f_mkfs(ctx->root, 0, work, sizeof(work));
        if (res != FR_OK) return XST_FAILURE;
        res = f_mount(ctx->fatfs, ctx->root, 1);
        if (res != FR_OK) return XST_FAILURE;
    }
    *(ctx->ready) = 1;
    xil_printf("%s: mounted %s\r\n", label ? label : "FS", ctx->root);
    return XST_SUCCESS;
}

int fs_shared_is_ready(const fs_shared_ctx_t *ctx)
{
    return ctx && ctx->ready && *(ctx->ready);
}

int fs_shared_fs_ls(const fs_shared_ctx_t *ctx, const char *path, int fd)
{
    if (!fs_shared_ensure_ready(ctx, fd)) return XST_FAILURE;
    if (!fs_shared_lock(ctx)) { write_str(fd, "FS busy\r\n"); return XST_FAILURE; }

    DIR dir;
    FILINFO fno;
    const char *target = path ? path : ctx->root;
    FRESULT res = f_opendir(&dir, target);
    if (res != FR_OK) {
        write_str(fd, "f_opendir failed\r\n");
        fs_shared_unlock(ctx);
        return XST_FAILURE;
    }
    char line[96];
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        char name[sizeof(fno.fname)];
        strncpy(name, fno.fname, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        normalize_name(name);
        int n = snprintf(line, sizeof(line), "%c %10lu %s\r\n",
            (fno.fattrib & AM_DIR) ? 'd' : '-',
            (unsigned long)fno.fsize,
            name);
        lwip_write(fd, line, n);
    }
    f_closedir(&dir);
    fs_shared_unlock(ctx);
    return XST_SUCCESS;
}

int fs_shared_fs_cat(const fs_shared_ctx_t *ctx, const char *path, int fd)
{
    if (!fs_shared_ensure_ready(ctx, fd)) return XST_FAILURE;
    if (!fs_shared_lock(ctx)) { write_str(fd, "FS busy\r\n"); return XST_FAILURE; }
    FIL file;
    FRESULT res = f_open(&file, path, FA_READ);
    if (res != FR_OK) {
        write_str(fd, "open failed\r\n");
        fs_shared_unlock(ctx);
        return XST_FAILURE;
    }
    char buf[128];
    UINT br = 0;
    do {
        res = f_read(&file, buf, sizeof(buf), &br);
        if (res != FR_OK) break;
        if (br) lwip_write(fd, buf, br);
    } while (br == sizeof(buf));
    f_close(&file);
    write_str(fd, "\r\n");
    fs_shared_unlock(ctx);
    return (res == FR_OK) ? XST_SUCCESS : XST_FAILURE;
}

int fs_shared_fs_touch(const fs_shared_ctx_t *ctx, const char *path)
{
    if (!ctx || !path || !fs_shared_is_ready(ctx)) return XST_FAILURE;
    if (!fs_shared_lock(ctx)) return XST_FAILURE;
    FIL file;
    FRESULT res = f_open(&file, path, FA_WRITE | FA_OPEN_ALWAYS);
    if (res == FR_OK) {
        res = f_truncate(&file);
        f_close(&file);
    }
    fs_shared_unlock(ctx);
    return (res == FR_OK) ? XST_SUCCESS : XST_FAILURE;
}

int fs_shared_fs_mkdir(const fs_shared_ctx_t *ctx, const char *path)
{
    if (!ctx || !path || !fs_shared_is_ready(ctx)) return XST_FAILURE;
    if (!fs_shared_lock(ctx)) return XST_FAILURE;
    FRESULT res = f_mkdir(path);
    fs_shared_unlock(ctx);
    return (res == FR_OK || res == FR_EXIST) ? XST_SUCCESS : XST_FAILURE;
}

int fs_shared_fs_rm(const fs_shared_ctx_t *ctx, const char *path)
{
    if (!ctx || !path || !fs_shared_is_ready(ctx)) return XST_FAILURE;
    if (!fs_shared_lock(ctx)) return XST_FAILURE;
    FRESULT res = f_unlink(path);
    fs_shared_unlock(ctx);
    return (res == FR_OK) ? XST_SUCCESS : XST_FAILURE;
}

int fs_shared_fs_is_dir(const fs_shared_ctx_t *ctx, const char *path)
{
    if (!ctx || !path || !fs_shared_is_ready(ctx)) return XST_FAILURE;
    if (!fs_shared_lock(ctx)) return XST_FAILURE;
    if (strcmp(path, ctx->root) == 0) { fs_shared_unlock(ctx); return XST_SUCCESS; }
    char norm[FS_PATH_MAX];
    strncpy(norm, path, sizeof(norm) - 1);
    norm[sizeof(norm) - 1] = '\0';
    size_t len = strlen(norm);
    if (len > 1 && norm[len - 1] == '/') { norm[len - 1] = '\0'; }
    FILINFO fno;
    FRESULT res = f_stat(norm, &fno);
    fs_shared_unlock(ctx);
    if (res != FR_OK) return XST_FAILURE;
    return (fno.fattrib & AM_DIR) ? XST_SUCCESS : XST_FAILURE;
}

int fs_shared_fs_complete(const fs_shared_ctx_t *ctx, const char *dir, const char *prefix,
                           char results[][FS_NAME_MAX], int max_results, int *out_count)
{
    if (out_count) *out_count = 0;
    if (!ctx || !dir || !results || max_results <= 0 || !fs_shared_is_ready(ctx)) return XST_FAILURE;
    if (!fs_shared_lock(ctx)) return XST_FAILURE;
    DIR d;
    FILINFO fno;
    FRESULT res = f_opendir(&d, dir);
    if (res != FR_OK) { fs_shared_unlock(ctx); return XST_FAILURE; }
    size_t prefix_len = prefix ? strlen(prefix) : 0;
    int cnt = 0;
    while (f_readdir(&d, &fno) == FR_OK && fno.fname[0]) {
        char name[sizeof(fno.fname)];
        strncpy(name, fno.fname, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        normalize_name(name);
        if (prefix_len && strncmp(name, prefix, prefix_len) != 0) continue;
        if (cnt < max_results) {
            strncpy(results[cnt], name, FS_NAME_MAX - 2);
            results[cnt][FS_NAME_MAX - 2] = '\0';
            if (fno.fattrib & AM_DIR) {
                size_t nlen = strlen(results[cnt]);
                if (nlen < FS_NAME_MAX - 1) {
                    results[cnt][nlen] = '/';
                    results[cnt][nlen + 1] = '\0';
                }
            }
        }
        cnt++;
    }
    f_closedir(&d);
    fs_shared_unlock(ctx);
    if (out_count) *out_count = cnt;
    return XST_SUCCESS;
}
