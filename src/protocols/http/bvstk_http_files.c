#include "protocols/http/bvstk_http_ui.h"
#include "shared/protocols/http/bvstk_http_input.h"
#include "shared/interfaces/bvstk_clock.h"
#include <stdio.h>
#include <string.h>

static int resolve(const http_request_t *r,http_conn_t *c,const char *vk,const char *pk,
                   char volume[12],char path[BVSTK_FS_PATH_CAPACITY],int write)
{
    char rel[BVSTK_FS_PATH_CAPACITY];
    if (!http_ui_param(r,c,vk,volume,12)||!http_ui_param(r,c,pk,rel,sizeof(rel))) return 0;
    if (!bvstk_http_path(rel)||(write&&!strcmp(rel,"/"))||bvstk_fs_path(volume,rel,path,BVSTK_FS_PATH_CAPACITY)!=BVSTK_FS_OK) {
        http_ui_error(c->fd,400,"invalid path or volume root");return 0;
    }
    return 1;
}
static unsigned legacy_result(bvstk_fs_result_t rc)
{
    switch(rc) {
    case BVSTK_FS_OK:return 0;case BVSTK_FS_NOT_FOUND:return 4;
    case BVSTK_FS_NOT_READY:case BVSTK_FS_UNSUPPORTED:return 3;
    case BVSTK_FS_EXISTS:return 8;case BVSTK_FS_DENIED:return 7;
    case BVSTK_FS_BUSY:return 16;case BVSTK_FS_INVALID:case BVSTK_FS_RANGE:return 6;
    default:return 1;
    }
}
static void storage_error(int fd,bvstk_fs_result_t rc)
{
    http_ui_error(fd,rc==BVSTK_FS_NOT_FOUND?404:rc==BVSTK_FS_EXISTS?409:
        rc==BVSTK_FS_NOT_READY||rc==BVSTK_FS_UNSUPPORTED?503:rc==BVSTK_FS_DENIED?403:
        rc==BVSTK_FS_BUSY?423:rc==BVSTK_FS_INVALID||rc==BVSTK_FS_RANGE||rc==BVSTK_FS_TYPE_ERROR?400:500,
        bvstk_fs_result_name(rc));
}
static void info_json(int fd, const char *name, const bvstk_fs_info_t *info)
{
    http_ui_text(fd, "{\"name\":"); http_ui_string(fd, name);
    /* This legacy FreeRTOS route retains its FAT date/time representation. */
    http_ui_printf(fd, ",\"directory\":%s,\"size\":%llu,\"date\":%u,\"time\":%u}",
        info->type == BVSTK_FS_ENTRY_DIR ? "true" : "false", (unsigned long long)info->size,
        (unsigned)(http_ui_backend()->fat_timestamps ? info->stamp >> 16 : 0U), (unsigned)(http_ui_backend()->fat_timestamps ? info->stamp & 0xffffU : 0U));
}
static void directory(const http_request_t *r, http_conn_t *c, const char *path)
{
    char number[16]; uint32_t offset = 0, limit = 64, seen = 0, count = 0;
    int q = bvstk_http_query(r->path, "offset", number, sizeof(number));
    if (q < 0 || (q && !bvstk_http_uint(number, 100000U, &offset))) { http_ui_error(c->fd, 400, "invalid offset"); return; }
    q = bvstk_http_query(r->path, "limit", number, sizeof(number));
    if (q < 0 || (q && (!bvstk_http_uint(number, 128U, &limit) || !limit))) { http_ui_error(c->fd, 400, "invalid limit"); return; }
    const bvstk_fs_backend_t *b = http_ui_backend()->storage()->reader;
    void *dir = NULL;
    bvstk_fs_result_t rc = b->dir_open(b->context, path, &dir);
    if (rc != BVSTK_FS_OK) { storage_error(c->fd, rc); return; }
    http_ui_header(c->fd, "application/json", -1); http_ui_text(c->fd, "{\"entries\":[");
    int more = 0;
    for (;;) {
        char name[BVSTK_FS_PATH_CAPACITY]; bvstk_fs_info_t info;
        rc = b->dir_read(b->context, dir, name, sizeof(name), &info);
        if (rc != BVSTK_FS_OK || !name[0]) break;
        if (seen++ < offset) continue;
        if (count == limit) { more = 1; break; }
        if (count++) http_ui_text(c->fd, ",");
        info_json(c->fd, name, &info);
    }
    b->dir_close(b->context, dir);
    http_ui_printf(c->fd, "],\"ok\":%s,\"error\":%u,\"next\":",
        rc == BVSTK_FS_OK ? "true" : "false", (unsigned)legacy_result(rc));
    if (more) http_ui_printf(c->fd, "%lu", (unsigned long)(offset + count)); else http_ui_text(c->fd, "null");
    http_ui_text(c->fd, "}\n");
}
static void transfer(const http_request_t *r, http_conn_t *c, const char *path)
{
    uint8_t buf[1024];
    const bvstk_fs_storage_t *s = http_ui_backend()->storage();
    if (!strcmp(r->method, "GET")) {
        char limit_text[16]; uint32_t limit = 0;
        int limited = bvstk_http_query(r->path, "max_bytes", limit_text, sizeof(limit_text));
        if (limited < 0 || (limited && !bvstk_http_uint(limit_text, UINT32_MAX, &limit))) {
            http_ui_error(c->fd, 400, "invalid read limit"); return;
        }
        bvstk_fs_reader_t reader;
        bvstk_fs_result_t rc = bvstk_fs_reader_open(&reader, s->reader, path, BVSTK_FS_FILE, 0);
        if (rc != BVSTK_FS_OK) { storage_error(c->fd, rc); return; }
        if (limited && reader.total_size > limit) {
            bvstk_fs_reader_close(&reader); http_ui_error(c->fd, 413, "file exceeds read limit"); return;
        }
        http_ui_header(c->fd, "application/octet-stream", (long long)reader.total_size);
        bool eof = false;
        while (!eof) {
            size_t n = 0;
            rc = bvstk_fs_reader_read(&reader, buf, sizeof(buf), &n, &eof);
            if (rc != BVSTK_FS_OK || (n && !http_ui_send(c->fd, buf, n))) break;
        }
        bvstk_fs_reader_close(&reader); return;
    }
    char overwrite[8]; int replace = bvstk_http_query(r->path, "overwrite", overwrite, sizeof(overwrite));
    if (replace < 0 || (replace && strcmp(overwrite, "true") && strcmp(overwrite, "false"))) {
        http_ui_error(c->fd, 400, "invalid overwrite flag"); return;
    }
    if (!r->has_content_length && !r->chunked) { http_ui_error(c->fd, 411, "length required"); return; }
    if (r->has_content_length && r->content_length > UINT32_MAX) { http_ui_error(c->fd, 413, "file too large"); return; }
    replace = replace && !strcmp(overwrite, "true");
    bvstk_fs_writer_t writer;
    bvstk_fs_result_t rc = bvstk_fs_writer_open(&writer, s, path, replace);
    if (rc != BVSTK_FS_OK) { storage_error(c->fd, rc); return; }
    while (rc == BVSTK_FS_OK) {
        int n = c->read_body(c->read_user, buf, sizeof(buf));
        if (n < 0 || n > (int)sizeof(buf)) { rc = BVSTK_FS_IO; break; }
        if (!n) break;
        rc = bvstk_fs_writer_write(&writer, buf, (size_t)n);
    }
    if (rc == BVSTK_FS_OK && r->has_content_length && writer.bytes != r->content_length) rc = BVSTK_FS_IO;
    bvstk_fs_result_t closed = rc == BVSTK_FS_OK ? bvstk_fs_writer_close(&writer) : bvstk_fs_writer_abort(&writer);
    if (rc == BVSTK_FS_OK) rc = closed;
    if (rc != BVSTK_FS_OK) { storage_error(c->fd, rc); return; }
    http_ui_header(c->fd, "application/json", -1);
    http_ui_printf(c->fd, "{\"ok\":true,\"bytes\":%llu}\n", (unsigned long long)writer.bytes);
}
int http_ui_files(const http_request_t *r, http_conn_t *c)
{
    char dev[12], dstdev[12];
    char path[BVSTK_FS_PATH_CAPACITY], dst[BVSTK_FS_PATH_CAPACITY], action[20], confirm[8];
    const char *end = strchr(r->path, '?'); size_t len = end ? (size_t)(end - r->path) : strlen(r->path);
    int get = !strcmp(r->method, "GET"), transfer_mode = 0;
    if (len == 15U && !strncmp(r->path, "/api/ui/volumes", len)) {
        if (get) http_ui_backend()->volumes(c->fd); else http_ui_error(c->fd, 405, "GET required"); return 1;
    }
    if (len == 13U && !strncmp(r->path, "/api/ui/sd-pl", len)) {
        if (get) http_ui_backend()->sd_info(c->fd); else http_ui_error(c->fd, 405, "GET required"); return 1;
    }
    if (len == 12U && !strncmp(r->path, "/api/ui/file", len)) transfer_mode = 1;
    else if (len != 13U || strncmp(r->path, "/api/ui/files", len)) return 0;
    if (!resolve(r, c, "volume", "path", dev, path, !get)) return 1;
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
        if (!resolve(r, c, "dst_volume", "dst", dstdev, dst, 1)) return 1;
        if (!strcmp(action, "rename") && strcmp(dev,dstdev)) { http_ui_error(c->fd, 400, "rename is within one volume"); return 1; }
    }
    /* The adapter serializes filesystem calls, releasing the volume mutex
     * before network I/O. A stalled client does not hold the filesystem. */
    const bvstk_fs_storage_t *s = http_ui_backend()->storage();
    if (transfer_mode) transfer(r, c, path);
    else if (get && !strcmp(action, "list")) directory(r, c, path);
    else if (get && !strcmp(action, "stat")) {
        bvstk_fs_info_t info; bvstk_fs_result_t rc = bvstk_fs_stat(s->reader, path, &info);
        if (rc == BVSTK_FS_OK) {
            const char *name = strrchr(path, '/');
            http_ui_header(c->fd, "application/json", -1); info_json(c->fd, name ? name + 1 : path, &info);
        } else storage_error(c->fd, rc);
    } else if (!get) {
        bvstk_fs_result_t rc;
        if (!strcmp(action, "copy")) rc = bvstk_fs_copy(s, path, dst, NULL);
        else if (!strcmp(action, "mkdir")) rc = bvstk_fs_mkdir(s, path);
        else if (!strcmp(action, "remove")) rc = bvstk_fs_remove(s, path);
        else if (!strcmp(action, "rename")) rc = bvstk_fs_rename(s, path, dst);
        else { http_ui_error(c->fd, 400, "unsupported file action"); return 1; }
        if (rc == BVSTK_FS_OK) http_ui_status(c->fd, BVSTK_OK); else storage_error(c->fd, rc);
    } else http_ui_error(c->fd, 400, "unsupported file action");
    return 1;
}
