#include "fs_shared.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "lwip/sockets.h"
#include "xil_printf.h"
#include "xstatus.h"

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

static const char *fs_shared_entry_name(const FILINFO *fno, char *out, size_t out_sz)
{
#if FF_USE_LFN
    const char *src = (fno->fname[0]) ? fno->fname : fno->altname;
#else
    const char *src = fno->fname;
#endif
    if (!src || src[0] == '\0') return NULL;
    strncpy(out, src, out_sz - 1);
    out[out_sz - 1] = '\0';
    return out;
}

int fs_shared_mount(fs_shared_ctx_t *ctx, const char *label)
{
    if (!ctx || !ctx->fatfs || !ctx->ready || !ctx->root) return XST_FAILURE;
    FRESULT res = f_mount(ctx->fatfs, ctx->root, 1);
    if (res != FR_OK) {
        BYTE work[FF_MAX_SS];
        res = f_mkfs(ctx->root, 0, 0, work, sizeof(work));
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
    char name[FF_MAX_LFN + 1];
    enum { LS_REPEAT_LIMIT = 4 };
    char last_name[FF_MAX_LFN + 1] = "";
    int same_count = 0;
    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK) break;
        if (fno.fname[0] == '\0') break;
        const char *entry = fs_shared_entry_name(&fno, name, sizeof(name));
        if (!entry) continue;
        normalize_name(name);
        if (name[0] == '\0') continue;
        if (strcmp(name, last_name) == 0) {
            ++same_count;
            if (same_count >= LS_REPEAT_LIMIT) {
                write_str(fd, "ls: stopped because entry repeated\r\n");
                break;
            }
        } else {
            same_count = 0;
            strncpy(last_name, name, sizeof(last_name) - 1);
            last_name[sizeof(last_name) - 1] = '\0';
        }
        int n = snprintf(line, sizeof(line), "%c %10lu %s\r\n",
            (fno.fattrib & AM_DIR) ? 'd' : '-',
            (unsigned long)fno.fsize,
            name);
        size_t to_write = (n < 0) ? 0 : (size_t)n;
        if (to_write >= sizeof(line)) to_write = sizeof(line) - 1;
        lwip_write(fd, line, to_write);
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

FRESULT fs_shared_fs_touch(const fs_shared_ctx_t *ctx, const char *path)
{
    if (!ctx || !path || !fs_shared_is_ready(ctx)) return FR_NOT_READY;
    if (!fs_shared_lock(ctx)) return FR_TIMEOUT;
    FIL file;
    FRESULT res = f_open(&file, path, FA_WRITE | FA_OPEN_ALWAYS);
    if (res == FR_OK) {
        res = f_truncate(&file);
        f_close(&file);
    }
    fs_shared_unlock(ctx);
    return res;
}

FRESULT fs_shared_fs_mkdir(const fs_shared_ctx_t *ctx, const char *path)
{
    if (!ctx || !path || !fs_shared_is_ready(ctx)) return FR_NOT_READY;
    if (!fs_shared_lock(ctx)) return FR_TIMEOUT;
    FRESULT res = f_mkdir(path);
    fs_shared_unlock(ctx);
    return res;
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
    if (res != FR_OK) {
        fs_shared_unlock(ctx);
        return XST_FAILURE;
    }
    size_t prefix_len = prefix ? strlen(prefix) : 0;
    int cnt = 0;
    char name[FF_MAX_LFN + 1];
    while (1) {
        res = f_readdir(&d, &fno);
        if (res != FR_OK) break;
        if (fno.fname[0] == '\0') break;
        const char *entry = fs_shared_entry_name(&fno, name, sizeof(name));
        if (!entry) continue;
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
        ++cnt;
    }
    f_closedir(&d);
    fs_shared_unlock(ctx);
    if (out_count) *out_count = cnt;
    return XST_SUCCESS;
}

static bool fs_shared_basename(const char *path, char *out, size_t out_sz)
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

static bool fs_shared_join_path(const char *parent, const char *child, char *out, size_t out_sz)
{
    if (!parent || !child || !out || out_sz == 0) return false;
    size_t parent_len = strlen(parent);
    bool has_slash = parent_len > 0 && parent[parent_len - 1] == '/';
    int needed = has_slash ?
                 snprintf(out, out_sz, "%s%s", parent, child) :
                 snprintf(out, out_sz, "%s/%s", parent, child);
    return (needed >= 0 && (size_t)needed < out_sz);
}

static bool fs_shared_is_subpath(const char *parent, const char *child)
{
    if (!parent || !child) return false;
    size_t parent_len = strlen(parent);
    size_t child_len = strlen(child);
    if (parent_len == 0 || child_len < parent_len) return false;
    if (strncmp(parent, child, parent_len) != 0) return false;
    if (child_len == parent_len) return true;
    if (parent[parent_len - 1] == '/') return true;
    return child[parent_len] == '/';
}

static int fs_shared_copy_file_locked(const char *src, const char *dst)
{
    FIL src_file;
    FRESULT res = f_open(&src_file, src, FA_READ);
    if (res != FR_OK) return XST_FAILURE;

    FIL dst_file;
    res = f_open(&dst_file, dst, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        f_close(&src_file);
        return XST_FAILURE;
    }

    char buf[1024];
    while (1) {
        UINT br = 0;
        res = f_read(&src_file, buf, sizeof(buf), &br);
        if (res != FR_OK) break;
        if (br == 0) break;
        UINT bw = 0;
        res = f_write(&dst_file, buf, br, &bw);
        if (res != FR_OK || bw != br) break;
    }

    f_close(&src_file);
    f_close(&dst_file);
    return (res == FR_OK) ? XST_SUCCESS : XST_FAILURE;
}

static bool fs_shared_prepare_file_target(const char *src, const char *dst, char *out, size_t out_sz)
{
    FILINFO dst_info;
    FRESULT res = f_stat(dst, &dst_info);
    if (res == FR_OK) {
        if (dst_info.fattrib & AM_DIR) {
            char base[FS_NAME_MAX];
            if (!fs_shared_basename(src, base, sizeof(base))) return false;
            return fs_shared_join_path(dst, base, out, out_sz);
        }
        strncpy(out, dst, out_sz);
        out[out_sz - 1] = '\0';
        return true;
    }
    if (res != FR_NO_FILE && res != FR_NO_PATH) return false;
    strncpy(out, dst, out_sz);
    out[out_sz - 1] = '\0';
    return true;
}

static int fs_shared_ensure_dir(const char *path)
{
    FILINFO info;
    FRESULT res = f_stat(path, &info);
    if (res == FR_OK) {
        return (info.fattrib & AM_DIR) ? XST_SUCCESS : XST_FAILURE;
    }
    res = f_mkdir(path);
    if (res == FR_OK || res == FR_EXIST) return XST_SUCCESS;
    return XST_FAILURE;
}

static int fs_shared_copy_directory_contents(const char *src, const char *dst)
{
    if (fs_shared_ensure_dir(dst) != XST_SUCCESS) return XST_FAILURE;

    DIR dir;
    FRESULT res = f_opendir(&dir, src);
    if (res != FR_OK) return XST_FAILURE;

    int ret = XST_SUCCESS;
    FILINFO fno;
    char entry_name[FF_MAX_LFN + 1];
    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK) {
            ret = XST_FAILURE;
            break;
        }
        const char *name = fs_shared_entry_name(&fno, entry_name, sizeof(entry_name));
        if (!name) break;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
            continue;
        }
        char src_entry[FS_PATH_MAX];
        char dst_entry[FS_PATH_MAX];
        if (!fs_shared_join_path(src, name, src_entry, sizeof(src_entry)) ||
            !fs_shared_join_path(dst, name, dst_entry, sizeof(dst_entry))) {
            ret = XST_FAILURE;
            break;
        }
        if (fno.fattrib & AM_DIR) {
            if (fs_shared_copy_directory_contents(src_entry, dst_entry) != XST_SUCCESS) {
                ret = XST_FAILURE;
                break;
            }
        } else {
            if (fs_shared_copy_file_locked(src_entry, dst_entry) != XST_SUCCESS) {
                ret = XST_FAILURE;
                break;
            }
        }
    }

    f_closedir(&dir);
    return ret;
}

static int fs_shared_remove_entry_recursive(const char *path)
{
    FILINFO info;
    FRESULT res = f_stat(path, &info);
    if (res != FR_OK) return XST_FAILURE;
    if (info.fattrib & AM_DIR) {
        DIR dir;
        res = f_opendir(&dir, path);
        if (res != FR_OK) return XST_FAILURE;
        FILINFO fno;
        char entry_name[FF_MAX_LFN + 1];
        int ret = XST_SUCCESS;
        while (ret == XST_SUCCESS) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK) {
                ret = XST_FAILURE;
                break;
            }
            const char *name = fs_shared_entry_name(&fno, entry_name, sizeof(entry_name));
            if (!name) break;
            if (name[0] == '.' && (name[1] == '\0' ||
                (name[1] == '.' && name[2] == '\0'))) {
                continue;
            }
            char child[FS_PATH_MAX];
            if (!fs_shared_join_path(path, name, child, sizeof(child))) {
                ret = XST_FAILURE;
                break;
            }
            if (fs_shared_remove_entry_recursive(child) != XST_SUCCESS) {
                ret = XST_FAILURE;
                break;
            }
        }
        f_closedir(&dir);
        if (ret != XST_SUCCESS) return XST_FAILURE;
        res = f_unlink(path);
        return (res == FR_OK) ? XST_SUCCESS : XST_FAILURE;
    }
    res = f_unlink(path);
    return (res == FR_OK) ? XST_SUCCESS : XST_FAILURE;
}

static int fs_shared_fs_rm_recursive(const fs_shared_ctx_t *ctx, const char *path)
{
    if (!ctx || !path || !fs_shared_is_ready(ctx)) return XST_FAILURE;
    if (!fs_shared_lock(ctx)) return XST_FAILURE;
    int ret = fs_shared_remove_entry_recursive(path);
    fs_shared_unlock(ctx);
    return ret;
}

static int fs_shared_fs_cp_locked(const char *src, const char *dst, bool recursive)
{
    FILINFO src_info;
    FRESULT res = f_stat(src, &src_info);
    if (res != FR_OK) return XST_FAILURE;
    bool src_is_dir = (src_info.fattrib & AM_DIR) != 0;
    if (src_is_dir && !recursive) return XST_FAILURE;

    char target[FS_PATH_MAX];
    if (src_is_dir) {
        FILINFO dst_info;
        res = f_stat(dst, &dst_info);
        if (res == FR_OK) {
            if (!(dst_info.fattrib & AM_DIR)) return XST_FAILURE;
            char base[FS_NAME_MAX];
            if (!fs_shared_basename(src, base, sizeof(base))) return XST_FAILURE;
            if (!fs_shared_join_path(dst, base, target, sizeof(target))) return XST_FAILURE;
        } else if (res == FR_NO_FILE || res == FR_NO_PATH) {
            strncpy(target, dst, sizeof(target));
            target[sizeof(target) - 1] = '\0';
        } else {
            return XST_FAILURE;
        }
        if (fs_shared_is_subpath(src, target)) return XST_FAILURE;
        return fs_shared_copy_directory_contents(src, target);
    }

    if (!fs_shared_prepare_file_target(src, dst, target, sizeof(target))) return XST_FAILURE;
    return fs_shared_copy_file_locked(src, target);
}

int fs_shared_fs_cp(const fs_shared_ctx_t *ctx, const char *src, const char *dst, bool recursive)
{
    if (!ctx || !src || !dst || !fs_shared_is_ready(ctx)) return XST_FAILURE;
    if (!fs_shared_lock(ctx)) return XST_FAILURE;
    int ret = fs_shared_fs_cp_locked(src, dst, recursive);
    fs_shared_unlock(ctx);
    return ret;
}

int fs_shared_fs_cp_between(const fs_shared_ctx_t *src_ctx, const fs_shared_ctx_t *dst_ctx,
                            const char *src, const char *dst, bool recursive)
{
    if (!src_ctx || !dst_ctx || !src || !dst) return XST_FAILURE;
    if (src_ctx == dst_ctx) return fs_shared_fs_cp(src_ctx, src, dst, recursive);
    if (!fs_shared_is_ready(src_ctx) || !fs_shared_is_ready(dst_ctx)) return XST_FAILURE;
    const fs_shared_ctx_t *first = (src_ctx < dst_ctx) ? src_ctx : dst_ctx;
    const fs_shared_ctx_t *second = (first == src_ctx) ? dst_ctx : src_ctx;
    if (!fs_shared_lock(first)) return XST_FAILURE;
    if (!fs_shared_lock(second)) {
        fs_shared_unlock(first);
        return XST_FAILURE;
    }
    int ret = fs_shared_fs_cp_locked(src, dst, recursive);
    fs_shared_unlock(second);
    fs_shared_unlock(first);
    return ret;
}

static int fs_shared_fs_mv_locked(const char *src, const char *dst)
{
    char target[FS_PATH_MAX];
    if (!fs_shared_prepare_file_target(src, dst, target, sizeof(target))) return XST_FAILURE;
    if (strcmp(src, target) == 0) return XST_SUCCESS;
    FILINFO src_info;
    FRESULT res = f_stat(src, &src_info);
    if (res != FR_OK) return XST_FAILURE;
    bool src_is_dir = (src_info.fattrib & AM_DIR) != 0;
    if (src_is_dir && fs_shared_is_subpath(src, target)) return XST_FAILURE;
    FILINFO dst_info;
    res = f_stat(target, &dst_info);
    if (res == FR_OK) {
        if (dst_info.fattrib & AM_DIR) return XST_FAILURE;
        res = f_unlink(target);
        if (res != FR_OK) return XST_FAILURE;
    } else if (res != FR_NO_FILE && res != FR_NO_PATH) {
        return XST_FAILURE;
    }
    res = f_rename(src, target);
    return (res == FR_OK) ? XST_SUCCESS : XST_FAILURE;
}

int fs_shared_fs_mv(const fs_shared_ctx_t *ctx, const char *src, const char *dst)
{
    if (!ctx || !src || !dst || !fs_shared_is_ready(ctx)) return XST_FAILURE;
    if (!fs_shared_lock(ctx)) return XST_FAILURE;
    int ret = fs_shared_fs_mv_locked(src, dst);
    fs_shared_unlock(ctx);
    return ret;
}

int fs_shared_fs_mv_between(const fs_shared_ctx_t *src_ctx, const fs_shared_ctx_t *dst_ctx,
                            const char *src, const char *dst, bool recursive)
{
    if (!src_ctx || !dst_ctx || !src || !dst) return XST_FAILURE;
    if (src_ctx == dst_ctx) return fs_shared_fs_mv(src_ctx, src, dst);
    if (!fs_shared_is_ready(src_ctx) || !fs_shared_is_ready(dst_ctx)) return XST_FAILURE;
    if (fs_shared_fs_cp_between(src_ctx, dst_ctx, src, dst, recursive) != XST_SUCCESS) return XST_FAILURE;
    int rm_result = recursive ? fs_shared_fs_rm_recursive(src_ctx, src) : fs_shared_fs_rm(src_ctx, src);
    return (rm_result == XST_SUCCESS) ? XST_SUCCESS : XST_FAILURE;
}
