#include "apps/freertos/services/http/http_ui.h"
#include "shared/protocols/http/bvstk_http_input.h"
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.h"
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.h"
#include "shared/interfaces/bvstk_clock.h"
#include <stdio.h>
#include <string.h>

static void fs_error(int fd, FRESULT st)
{
    char message[64];
    snprintf(message, sizeof(message), "filesystem error %u", (unsigned)st);
    http_ui_error(fd, st == FR_NO_FILE || st == FR_NO_PATH ? 404 : st == FR_EXIST ? 409 :
        st == FR_DENIED || st == FR_WRITE_PROTECTED ? 403 :
        st == FR_NOT_READY || st == FR_NOT_ENABLED || st == FR_NO_FILESYSTEM ? 503 :
        st == FR_TIMEOUT || st == FR_LOCKED ? 423 : st == FR_INVALID_NAME ? 400 : 500, message);
}
static int resolve(const http_request_t *r, http_conn_t *c, const char *vol_key, const char *path_key,
                   const fs_device_info_t **dev, char path[FS_PATH_MAX], int write)
{
    char vol[12], rel[FS_PATH_MAX];
    if (!http_ui_param(r, c, vol_key, vol, sizeof(vol)) || !http_ui_param(r, c, path_key, rel, sizeof(rel))) return 0;
    if (!bvstk_http_path(rel) || (write && !strcmp(rel, "/"))) { http_ui_error(c->fd, 400, "invalid path or volume root"); return 0; }
    *dev = fs_device_by_name(vol);
    if (!*dev || !fs_shared_is_ready((*dev)->ctx)) { http_ui_error(c->fd, 503, "volume is not mounted"); return 0; }
    int n = snprintf(path, FS_PATH_MAX, "%s%s", (*dev)->ctx->root, rel + 1);
    if (n < 0 || n >= FS_PATH_MAX) { http_ui_error(c->fd, 400, "path too long"); return 0; }
    return 1;
}
static void volumes(int fd)
{
    static struct { uint64_t total, free, at; int valid; } cached[3];
    static const char *const names[] = {"flash", "sd", "sd-pl"};
    uint64_t now = bvstk_platform_now_ms();
    http_ui_header(fd, "application/json", -1); http_ui_text(fd, "{\"volumes\":[");
    for (unsigned i = 0; i < 3U; ++i) {
        const fs_device_info_t *dev = fs_device_by_name(names[i]);
        int ready = dev && fs_shared_is_ready(dev->ctx);
        unsigned fat = 0;
        if (!ready) cached[i].valid = 0;
        if (ready && i < 2U && (!cached[i].valid || now - cached[i].at > 30000U) && http_ui_lock(dev->ctx)) {
            char drive[3] = {dev->ctx->root[0], ':', 0}; FATFS *fs = NULL; DWORD free_clusters;
            cached[i].valid = 0;
            if (f_getfree(drive, &free_clusters, &fs) == FR_OK && fs && fs->n_fatent >= 2U) {
                cached[i].total = (uint64_t)(fs->n_fatent - 2U) * fs->csize * 512U;
                cached[i].free = (uint64_t)free_clusters * fs->csize * 512U;
                cached[i].valid = 1; cached[i].at = now;
            }
            http_ui_unlock(dev->ctx);
        }
        if (i == 2U && ready) {
            const bvstk_sd_volume_t *v = bvstk_sd_pl_volume();
            if (v) { cached[i].total = (uint64_t)v->sectors * 512U; fat = v->fat_bits; }
        }
        http_ui_printf(fd, "%s{\"name\":\"%s\",\"ready\":%s,\"fat_bits\":%u,\"total_bytes\":",
            i ? "," : "", names[i], ready ? "true" : "false", fat);
        if (ready && (cached[i].valid || i == 2U)) http_ui_printf(fd, "%llu", (unsigned long long)cached[i].total);
        else http_ui_text(fd, "null");
        http_ui_text(fd, ",\"free_bytes\":");
        if (ready && cached[i].valid && i < 2U) http_ui_printf(fd, "%llu", (unsigned long long)cached[i].free);
        else http_ui_text(fd, "null");
        http_ui_text(fd, ",\"format_allowed\":false}");
    }
    http_ui_text(fd, "]}\n");
}
static void sd_info(int fd)
{
    bvstk_sd_card_info_t info; bvstk_sd_service_t *s = bvstk_sd_master_freertos_service();
    bvstk_status_t st = bvstk_sd_service_info(s, &info);
    if (st != BVSTK_OK) { http_ui_status(fd, st); return; }
    http_ui_header(fd, "application/json", -1);
    http_ui_printf(fd, "{\"ready\":%s,\"mounted\":%s,\"filesystem_owned\":%s,\"ocr\":%lu,\"block_addressed\":%s,\"last_lba\":%lu,\"last_error\":",
        info.ready ? "true" : "false", bvstk_sd_pl_is_ready() ? "true" : "false",
        info.filesystem_owned ? "true" : "false", (unsigned long)info.ocr,
        info.block_addressed ? "true" : "false", (unsigned long)info.last.lba);
    http_ui_string(fd, bvstk_status_string(info.last.status));
    const bvstk_sd_volume_t *v = bvstk_sd_pl_volume();
    if (v) http_ui_printf(fd, ",\"fat_bits\":%u,\"volume_lba\":%lu,\"volume_sectors\":%lu",
        (unsigned)v->fat_bits, (unsigned long)v->first_lba, (unsigned long)v->sectors);
    http_ui_text(fd, ",\"format_allowed\":false}\n");
}
static void info_json(int fd, const FILINFO *i)
{
    http_ui_text(fd, "{\"name\":"); http_ui_string(fd, i->fname);
    http_ui_printf(fd, ",\"directory\":%s,\"size\":%llu,\"date\":%u,\"time\":%u}",
        i->fattrib & AM_DIR ? "true" : "false", (unsigned long long)i->fsize,
        (unsigned)i->fdate, (unsigned)i->ftime);
}
static void directory(const http_request_t *r, http_conn_t *c, const char *path)
{
    char number[16]; uint32_t offset = 0, limit = 64, seen = 0, count = 0;
    int q = bvstk_http_query(r->path, "offset", number, sizeof(number));
    if (q < 0 || (q && !bvstk_http_uint(number, 100000U, &offset))) { http_ui_error(c->fd, 400, "invalid offset"); return; }
    q = bvstk_http_query(r->path, "limit", number, sizeof(number));
    if (q < 0 || (q && (!bvstk_http_uint(number, 128U, &limit) || !limit))) { http_ui_error(c->fd, 400, "invalid limit"); return; }
    DIR dir; FILINFO fi; FRESULT st = f_opendir(&dir, path);
    if (st != FR_OK) { fs_error(c->fd, st); return; }
    http_ui_header(c->fd, "application/json", -1); http_ui_text(c->fd, "{\"entries\":[");
    int more = 0;
    for (;;) {
        st = f_readdir(&dir, &fi);
        if (st != FR_OK || !fi.fname[0]) break;
        if (!strcmp(fi.fname, ".") || !strcmp(fi.fname, "..")) continue;
        if (seen++ < offset) continue;
        if (count == limit) { more = 1; break; }
        if (count++) http_ui_text(c->fd, ",");
        info_json(c->fd, &fi);
    }
    FRESULT close = f_closedir(&dir);
    if (st == FR_OK) st = close;
    http_ui_printf(c->fd, "],\"ok\":%s,\"error\":%u,\"next\":", st == FR_OK ? "true" : "false", (unsigned)st);
    if (more) http_ui_printf(c->fd, "%lu", (unsigned long)(offset + count)); else http_ui_text(c->fd, "null");
    http_ui_text(c->fd, "}\n");
}
static void transfer(const http_request_t *r, http_conn_t *c, const char *path)
{
    FIL f; FRESULT st; char buf[1024]; UINT n; uint64_t written = 0;
    int writing = !strcmp(r->method, "PUT");
    if (!writing) {
        char limit_text[16]; uint32_t limit = 0;
        int limited = bvstk_http_query(r->path, "max_bytes", limit_text, sizeof(limit_text));
        if (limited < 0 || (limited && !bvstk_http_uint(limit_text, UINT32_MAX, &limit))) {
            http_ui_error(c->fd, 400, "invalid read limit"); return;
        }
        st = f_open(&f, path, FA_READ);
        if (st != FR_OK) { fs_error(c->fd, st); return; }
        if (limited && f_size(&f) > limit) {
            (void)f_close(&f); http_ui_error(c->fd, 413, "file exceeds read limit"); return;
        }
        http_ui_header(c->fd, "application/octet-stream", (long long)f_size(&f));
        for (;;) {
            st = f_read(&f, buf, sizeof(buf), &n);
            if (st != FR_OK || !n || !http_ui_send(c->fd, buf, n)) break;
        }
        (void)f_close(&f); return;
    }
    char overwrite[8]; int replace = bvstk_http_query(r->path, "overwrite", overwrite, sizeof(overwrite));
    if (replace < 0 || (replace && strcmp(overwrite, "true") && strcmp(overwrite, "false"))) { http_ui_error(c->fd, 400, "invalid overwrite flag"); return; }
    if (!r->has_content_length && !r->chunked) { http_ui_error(c->fd, 411, "length required"); return; }
    if (r->has_content_length && r->content_length > UINT32_MAX) { http_ui_error(c->fd, 413, "file too large"); return; }
    replace = replace && !strcmp(overwrite, "true");
    st = f_open(&f, path, FA_WRITE | (replace ? FA_OPEN_ALWAYS : FA_CREATE_NEW));
    if (st != FR_OK) { fs_error(c->fd, st); return; }
    if (replace) st = f_truncate(&f);
    while (st == FR_OK) {
        int got = c->read_body(c->read_user, buf, sizeof(buf));
        if (got < 0) { st = FR_INT_ERR; break; }
        if (!got) break;
        if (written + (uint64_t)got > UINT32_MAX) { st = FR_INVALID_PARAMETER; break; }
        st = f_write(&f, buf, (UINT)got, &n);
        if (st == FR_OK && n != (UINT)got) st = FR_DISK_ERR;
        written += n;
    }
    if (st == FR_OK && r->has_content_length && written != r->content_length) st = FR_INT_ERR;
    if (st == FR_OK) st = f_sync(&f);
    FRESULT close = f_close(&f); if (st == FR_OK) st = close;
    /* Partial files are deliberately retained on error; never retry/truncate automatically. */
    if (st != FR_OK) { fs_error(c->fd, st); return; }
    http_ui_header(c->fd, "application/json", -1);
    http_ui_printf(c->fd, "{\"ok\":true,\"bytes\":%llu}\n", (unsigned long long)written);
}
static void copy_file(int fd, const char *from, const char *to)
{
    FIL in, out; FILINFO info; char b[1024]; UINT nr, nw;
    FRESULT st = f_stat(from, &info);
    if (st != FR_OK) { fs_error(fd, st); return; }
    if (info.fattrib & AM_DIR) { http_ui_error(fd, 400, "only files can be copied"); return; }
    st = f_open(&in, from, FA_READ);
    if (st != FR_OK) { fs_error(fd, st); return; }
    st = f_open(&out, to, FA_WRITE | FA_CREATE_NEW);
    if (st != FR_OK) { (void)f_close(&in); fs_error(fd, st); return; }
    do {
        st = f_read(&in, b, sizeof(b), &nr);
        if (st != FR_OK || !nr) break;
        st = f_write(&out, b, nr, &nw);
        if (st == FR_OK && nw != nr) st = FR_DISK_ERR;
    } while (st == FR_OK);
    if (st == FR_OK) st = f_sync(&out);
    FRESULT close = f_close(&out); if (st == FR_OK) st = close;
    close = f_close(&in); if (st == FR_OK) st = close;
    if (st == FR_OK) http_ui_status(fd, BVSTK_OK); else fs_error(fd, st);
}
int http_ui_files(const http_request_t *r, http_conn_t *c)
{
    const fs_device_info_t *dev, *dstdev = NULL;
    const fs_shared_ctx_t *first, *second = NULL;
    char path[FS_PATH_MAX], dst[FS_PATH_MAX], action[20], confirm[8];
    const char *end = strchr(r->path, '?'); size_t len = end ? (size_t)(end - r->path) : strlen(r->path);
    int get = !strcmp(r->method, "GET"), transfer_mode = 0;
    if (len == 15U && !strncmp(r->path, "/api/ui/volumes", len)) {
        if (get) volumes(c->fd); else http_ui_error(c->fd, 405, "GET required"); return 1;
    }
    if (len == 13U && !strncmp(r->path, "/api/ui/sd-pl", len)) {
        if (get) sd_info(c->fd); else http_ui_error(c->fd, 405, "GET required"); return 1;
    }
    if (len == 12U && !strncmp(r->path, "/api/ui/file", len)) transfer_mode = 1;
    else if (len != 13U || strncmp(r->path, "/api/ui/files", len)) return 0;
    if (!resolve(r, c, "volume", "path", &dev, path, !get)) return 1;
    if (!get) {
        if (bvstk_http_query(r->path, "confirm", confirm, sizeof(confirm)) != 1 || strcmp(confirm, "true")) {
            http_ui_error(c->fd, 400, "confirmation required"); return 1;
        }
    }
    if (transfer_mode) strcpy(action, "transfer");
    else {
        int q = bvstk_http_query(r->path, "action", action, sizeof(action));
        if (get && !q) strcpy(action, "list");
        else if (q != 1) { http_ui_error(c->fd, 400, "invalid action"); return 1; }
    }
    if (!get && (!strcmp(action, "copy") || !strcmp(action, "rename"))) {
        if (!resolve(r, c, "dst_volume", "dst", &dstdev, dst, 1)) return 1;
        if (!strcmp(action, "rename") && dev != dstdev) { http_ui_error(c->fd, 400, "rename is within one volume"); return 1; }
    }
    first = dev->ctx;
    if (dstdev && dstdev->ctx != first) {
        second = dstdev->ctx;
        if ((uintptr_t)second < (uintptr_t)first) { const fs_shared_ctx_t *t = first; first = second; second = t; }
    }
    if (!http_ui_lock(first)) { http_ui_error(c->fd, 423, "volume busy"); return 1; }
    if (second && !http_ui_lock(second)) { http_ui_unlock(first); http_ui_error(c->fd, 423, "destination busy"); return 1; }
    if (transfer_mode) transfer(r, c, path);
    else if (get && !strcmp(action, "list")) directory(r, c, path);
    else if (get && !strcmp(action, "stat")) {
        FILINFO info; FRESULT st = f_stat(path, &info);
        if (st == FR_OK) { http_ui_header(c->fd, "application/json", -1); info_json(c->fd, &info); }
        else fs_error(c->fd, st);
    } else if (!get && !strcmp(action, "copy")) copy_file(c->fd, path, dst);
    else if (!get && (!strcmp(action, "mkdir") || !strcmp(action, "remove") || !strcmp(action, "rename"))) {
        FRESULT st = !strcmp(action, "mkdir") ? f_mkdir(path) : !strcmp(action, "remove") ? f_unlink(path) : f_rename(path, dst);
        if (st == FR_OK) http_ui_status(c->fd, BVSTK_OK); else fs_error(c->fd, st);
    } else http_ui_error(c->fd, 400, "unsupported file action");
    if (second) http_ui_unlock(second);
    http_ui_unlock(first); return 1;
}
