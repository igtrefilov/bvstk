#include "services/fs/bvstk_fs_storage.h"

#include <stdio.h>
#include <string.h>

enum { UPLOAD_SLOTS = 16 };

static bvstk_fs_result_t checked(const char *path, char out[BVSTK_FS_PATH_CAPACITY], bool mutation)
{
    size_t size = 0;
    if (!path) return BVSTK_FS_INVALID;
    while (size < BVSTK_FS_PATH_CAPACITY && path[size]) ++size;
    bvstk_fs_result_t rc = bvstk_fs_validate_path((const uint8_t *)path, size,
                                                 out, BVSTK_FS_PATH_CAPACITY);
    if (rc != BVSTK_FS_OK) return rc;
    const char *colon = strchr(out, ':');
    if (mutation && !colon[2]) return BVSTK_FS_INVALID;
    if (mutation && bvstk_fs_upload_reserved(out)) return BVSTK_FS_DENIED;
    return BVSTK_FS_OK;
}

bvstk_fs_result_t bvstk_fs_path(const char *volume, const char *relative, char *out, size_t capacity)
{
    char path[BVSTK_FS_PATH_CAPACITY];
    if (!volume || !relative || relative[0] != '/' || !out) return BVSTK_FS_INVALID;
    int n = snprintf(path, sizeof(path), "%s:%s", volume, relative);
    if (n < 0 || (size_t)n >= sizeof(path)) return BVSTK_FS_RANGE;
    return bvstk_fs_validate_path((const uint8_t *)path, (size_t)n, out, capacity);
}

bvstk_fs_result_t bvstk_fs_stat(const bvstk_fs_backend_t *b, const char *path, bvstk_fs_info_t *info)
{
    char canonical[BVSTK_FS_PATH_CAPACITY];
    bvstk_fs_result_t rc = checked(path, canonical, false);
    if (rc != BVSTK_FS_OK) return rc;
    if (!b || !b->stat || !info) return BVSTK_FS_INVALID;
    return b->stat(b->context, canonical, info);
}

static bool usable(const bvstk_fs_storage_t *s)
{
    return s && s->reader;
}

bvstk_fs_result_t bvstk_fs_mkdir(const bvstk_fs_storage_t *s, const char *path)
{
    char canonical[BVSTK_FS_PATH_CAPACITY];
    bvstk_fs_result_t rc = checked(path, canonical, true);
    if (rc != BVSTK_FS_OK) return rc;
    if (!usable(s) || !s->mkdir) return BVSTK_FS_UNSUPPORTED;
    return s->mkdir(s->reader->context, canonical);
}

bvstk_fs_result_t bvstk_fs_mkdirs(const bvstk_fs_storage_t *s, const char *path)
{
    char canonical[BVSTK_FS_PATH_CAPACITY];
    bvstk_fs_info_t info;
    bvstk_fs_result_t rc = checked(path, canonical, false);
    if (rc != BVSTK_FS_OK) return rc;
    char *start = strchr(canonical, ':') + 2;
    if (!*start) {
        rc = bvstk_fs_stat(s ? s->reader : NULL, canonical, &info);
        return rc == BVSTK_FS_OK && info.type != BVSTK_FS_ENTRY_DIR ? BVSTK_FS_TYPE_ERROR : rc;
    }
    for (char *p = start; ; ++p) {
        if (*p && *p != '/') continue;
        char saved = *p;
        *p = 0;
        rc = bvstk_fs_mkdir(s, canonical);
        if (rc == BVSTK_FS_EXISTS) {
            rc = bvstk_fs_stat(s->reader, canonical, &info);
            if (rc == BVSTK_FS_OK && info.type != BVSTK_FS_ENTRY_DIR) rc = BVSTK_FS_TYPE_ERROR;
        }
        *p = saved;
        if (rc != BVSTK_FS_OK || !saved) return rc;
    }
}

bvstk_fs_result_t bvstk_fs_remove(const bvstk_fs_storage_t *s, const char *path)
{
    char canonical[BVSTK_FS_PATH_CAPACITY];
    bvstk_fs_result_t rc = checked(path, canonical, true);
    if (rc != BVSTK_FS_OK) return rc;
    if (!usable(s) || !s->remove) return BVSTK_FS_UNSUPPORTED;
    return s->remove(s->reader->context, canonical);
}

bvstk_fs_result_t bvstk_fs_rename(const bvstk_fs_storage_t *s, const char *from, const char *to)
{
    char a[BVSTK_FS_PATH_CAPACITY], b[BVSTK_FS_PATH_CAPACITY];
    bvstk_fs_result_t rc = checked(from, a, true);
    if (rc == BVSTK_FS_OK) rc = checked(to, b, true);
    if (rc != BVSTK_FS_OK) return rc;
    size_t n = (size_t)(strchr(a, ':') - a);
    if ((size_t)(strchr(b, ':') - b) != n || strncmp(a, b, n)) return BVSTK_FS_INVALID;
    if (!usable(s) || !s->rename) return BVSTK_FS_UNSUPPORTED;
    return s->rename(s->reader->context, a, b);
}

bvstk_fs_result_t bvstk_fs_writer_open(bvstk_fs_writer_t *w,
    const bvstk_fs_storage_t *s, const char *path, bool overwrite)
{
    char canonical[BVSTK_FS_PATH_CAPACITY];
    if (!w) return BVSTK_FS_INVALID;
    memset(w, 0, sizeof(*w));
    w->storage = s;
    w->result = checked(path, canonical, true);
    if (w->result != BVSTK_FS_OK) return w->result;
    if (!usable(s) || !s->create || !s->write || !s->finish)
        return w->result = BVSTK_FS_UNSUPPORTED;
    w->result = s->create(s->reader->context, canonical, overwrite, &w->handle);
    if (w->result == BVSTK_FS_OK && !w->handle) w->result = BVSTK_FS_IO;
    return w->result;
}

bvstk_fs_result_t bvstk_fs_writer_append(bvstk_fs_writer_t *w,
    const bvstk_fs_storage_t *s, const char *path)
{
    char canonical[BVSTK_FS_PATH_CAPACITY];
    if (!w) return BVSTK_FS_INVALID;
    memset(w, 0, sizeof(*w)); w->storage = s;
    w->result = checked(path, canonical, true);
    if (w->result != BVSTK_FS_OK) return w->result;
    if (!usable(s) || !s->append || !s->write || !s->finish)
        return w->result = BVSTK_FS_UNSUPPORTED;
    w->result = s->append(s->reader->context, canonical, &w->handle);
    if (w->result == BVSTK_FS_OK && !w->handle) w->result = BVSTK_FS_IO;
    return w->result;
}

bvstk_fs_result_t bvstk_fs_writer_write(bvstk_fs_writer_t *w, const void *data, size_t size)
{
    if (!w || !w->handle) return BVSTK_FS_INVALID;
    if (!data && size) return w->result = BVSTK_FS_INVALID;
    if (w->result != BVSTK_FS_OK) return w->result;
    if (size > UINT32_MAX - w->bytes) return w->result = BVSTK_FS_RANGE;
    if (!size) return BVSTK_FS_OK;
    size_t count = 0;
    w->result = w->storage->write(w->storage->reader->context, w->handle, data, size, &count);
    if (count > size) return w->result = BVSTK_FS_IO;
    w->bytes += count;
    if (w->result == BVSTK_FS_OK && count != size) w->result = BVSTK_FS_IO;
    return w->result;
}

static bvstk_fs_result_t finish(bvstk_fs_writer_t *w, bool sync)
{
    if (!w) return BVSTK_FS_INVALID;
    if (!w->handle) return w->result;
    bvstk_fs_result_t rc = w->storage->finish(w->storage->reader->context, w->handle,
                                            sync && w->result == BVSTK_FS_OK);
    w->handle = NULL;
    if (w->result == BVSTK_FS_OK) w->result = rc;
    return w->result;
}

bvstk_fs_result_t bvstk_fs_writer_close(bvstk_fs_writer_t *w) { return finish(w, true); }
bvstk_fs_result_t bvstk_fs_writer_abort(bvstk_fs_writer_t *w) { return finish(w, false); }

bool bvstk_fs_upload_supported(const bvstk_fs_storage_t *s)
{
    return usable(s) && s->reader->stat && s->create && s->write && s->finish &&
           s->remove && s->publish;
}

bvstk_fs_result_t bvstk_fs_upload_begin(bvstk_fs_upload_t *u, const bvstk_fs_storage_t *s,
    const char *path, uint64_t size, bool overwrite)
{
    if (!u) return BVSTK_FS_INVALID;
    if (u->writer.handle) return BVSTK_FS_BUSY;
    if (!bvstk_fs_upload_supported(s)) return BVSTK_FS_UNSUPPORTED;
    if (size > UINT32_MAX) return BVSTK_FS_RANGE;
    memset(u, 0, sizeof(*u));
    bvstk_fs_result_t rc = checked(path, u->destination, true);
    if (rc != BVSTK_FS_OK) return rc;
    bvstk_fs_info_t info;
    void *ctx = s->reader->context;
    rc = s->reader->stat(ctx, u->destination, &info);
    if (rc == BVSTK_FS_OK) {
        if (info.type != BVSTK_FS_ENTRY_FILE) return BVSTK_FS_TYPE_ERROR;
        if (!overwrite) return BVSTK_FS_EXISTS;
        if (info.attributes & 1U) return BVSTK_FS_DENIED;
    } else {
        if (rc != BVSTK_FS_NOT_FOUND) return rc;
        /* The stage lives at the root. A missing destination directory must
         * still fail at BEGIN, before the client streams its file. */
        char *slash = strrchr(u->destination, '/');
        if (slash[-1] != ':') {
            *slash = '\0';
            rc = s->reader->stat(ctx, u->destination, &info);
            *slash = '/';
            if (rc != BVSTK_FS_OK) return rc;
            if (info.type != BVSTK_FS_ENTRY_DIR) return BVSTK_FS_TYPE_ERROR;
        }
    }

    u->expected_size = size;
    u->overwrite = overwrite;
    u->writer.storage = s;
    size_t root = (size_t)(strchr(u->destination, ':') + 2 - u->destination);
    memcpy(u->temporary, u->destination, root);
    for (unsigned i = 0; i < UPLOAD_SLOTS; ++i) {
        snprintf(u->temporary + root, sizeof(u->temporary) - root, "BVU%05u.OLD", i);
        /* Occupied slots may predate this boot. Never remove or overwrite
         * another upload's stage or its only surviving rollback file. */
        rc = s->reader->stat(ctx, u->temporary, &info);
        if (rc == BVSTK_FS_OK) continue;
        if (rc != BVSTK_FS_NOT_FOUND) return rc;
        memcpy(u->temporary + root + 9U, "TMP", 4);
        rc = s->reader->stat(ctx, u->temporary, &info);
        if (rc == BVSTK_FS_OK) continue;
        if (rc != BVSTK_FS_NOT_FOUND) return rc;
        rc = s->create(ctx, u->temporary, false, &u->writer.handle);
        if (rc == BVSTK_FS_EXISTS) continue;
        if (rc != BVSTK_FS_OK) return rc;
        return u->writer.handle ? BVSTK_FS_OK : BVSTK_FS_IO;
    }
    return BVSTK_FS_BUSY;
}

bvstk_fs_result_t bvstk_fs_upload_write(bvstk_fs_upload_t *u, const void *data, size_t size)
{
    if (!u || !u->writer.handle) return BVSTK_FS_INVALID;
    if (u->writer.result != BVSTK_FS_OK) return u->writer.result;
    if (size > u->expected_size - u->writer.bytes) return BVSTK_FS_RANGE;
    return bvstk_fs_writer_write(&u->writer, data, size);
}

static bvstk_fs_result_t discard_stage(bvstk_fs_upload_t *u, bvstk_fs_result_t rc)
{
    const bvstk_fs_storage_t *s = u->writer.storage;
    bvstk_fs_result_t cleanup = s->remove(s->reader->context, u->temporary);
    return cleanup == BVSTK_FS_OK || cleanup == BVSTK_FS_NOT_FOUND ? rc : cleanup;
}

bvstk_fs_result_t bvstk_fs_upload_abort(bvstk_fs_upload_t *u)
{
    if (!u) return BVSTK_FS_INVALID;
    if (!u->writer.handle) return BVSTK_FS_OK;
    u->writer.result = BVSTK_FS_OK; /* Report the outcome of cleanup. */
    return discard_stage(u, bvstk_fs_writer_abort(&u->writer));
}

bvstk_fs_result_t bvstk_fs_upload_commit(bvstk_fs_upload_t *u)
{
    if (!u || !u->writer.handle) return BVSTK_FS_INVALID;
    if (u->writer.result == BVSTK_FS_OK && u->writer.bytes != u->expected_size)
        u->writer.result = BVSTK_FS_RANGE;
    bvstk_fs_result_t rc = bvstk_fs_writer_close(&u->writer);
    bool consumed = false;
    const bvstk_fs_storage_t *s = u->writer.storage;
    if (rc == BVSTK_FS_OK)
        rc = s->publish(s->reader->context, u->temporary, u->destination, u->overwrite, &consumed);
    return consumed ? rc : discard_stage(u, rc);
}

bvstk_fs_result_t bvstk_fs_copy(const bvstk_fs_storage_t *s,
    const char *from, const char *to, uint64_t *bytes)
{
    bvstk_fs_info_t before, after;
    bvstk_fs_writer_t writer;
    uint8_t buffer[1024];
    void *file = NULL;
    uint64_t size = 0, read_bytes = 0;
    char source[BVSTK_FS_PATH_CAPACITY];
    if (bytes) *bytes = 0;
    bvstk_fs_result_t rc = checked(from, source, false);
    if (rc == BVSTK_FS_OK) rc = bvstk_fs_stat(s ? s->reader : NULL, source, &before);
    if (rc != BVSTK_FS_OK) return rc;
    if (before.type != BVSTK_FS_ENTRY_FILE) return BVSTK_FS_TYPE_ERROR;
    if (before.size > UINT32_MAX) return BVSTK_FS_RANGE;
    const bvstk_fs_backend_t *b = s->reader;
    if (!b->file_open || !b->file_read || !b->file_close) return BVSTK_FS_UNSUPPORTED;
    rc = b->file_open(b->context, source, &file, &size);
    if (rc != BVSTK_FS_OK) return rc;
    if (size != before.size) { b->file_close(b->context, file); return BVSTK_FS_CHANGED; }
    rc = bvstk_fs_writer_open(&writer, s, to, false);
    while (rc == BVSTK_FS_OK) {
        size_t count = 0;
        rc = b->file_read(b->context, file, buffer, sizeof(buffer), &count);
        if (rc != BVSTK_FS_OK) break;
        if (count > sizeof(buffer) || count > size - read_bytes) { rc = BVSTK_FS_CHANGED; break; }
        if (!count) {
            if (read_bytes != size) rc = BVSTK_FS_CHANGED;
            break;
        }
        read_bytes += count;
        rc = bvstk_fs_writer_write(&writer, buffer, count);
    }
    b->file_close(b->context, file);
    if (rc == BVSTK_FS_OK) {
        rc = bvstk_fs_stat(b, source, &after);
        if (rc == BVSTK_FS_NOT_FOUND) rc = BVSTK_FS_CHANGED;
        if (rc == BVSTK_FS_OK && (after.size != before.size || after.stamp != before.stamp ||
                                  after.type != before.type)) rc = BVSTK_FS_CHANGED;
    }
    if (rc == BVSTK_FS_OK) rc = bvstk_fs_writer_close(&writer);
    else (void)bvstk_fs_writer_abort(&writer);
    if (bytes) *bytes = writer.bytes;
    return rc;
}

bvstk_fs_result_t bvstk_fs_save(const bvstk_fs_storage_t *s,
    const char *path, const void *data, size_t size)
{
    char canonical[BVSTK_FS_PATH_CAPACITY], temporary[BVSTK_FS_PATH_CAPACITY];
    char backup[BVSTK_FS_PATH_CAPACITY];
    bvstk_fs_writer_t w;
    bvstk_fs_result_t rc = checked(path, canonical, true);
    if (rc != BVSTK_FS_OK) return rc;
    if (!data && size) return BVSTK_FS_INVALID;
    if (size > UINT32_MAX) return BVSTK_FS_RANGE;
    if (!usable(s) || !s->install || !s->remove) return BVSTK_FS_UNSUPPORTED;
    int n = snprintf(backup, sizeof(backup), "%s.bak", canonical);
    if (n < 0 || (size_t)n >= sizeof(backup)) return BVSTK_FS_RANGE;
    for (unsigned i = 0; ; ++i) {
        n = snprintf(temporary, sizeof(temporary), "%s.tmp%u", canonical, i);
        if (n < 0 || (size_t)n >= sizeof(temporary)) return BVSTK_FS_RANGE;
        rc = bvstk_fs_writer_open(&w, s, temporary, false);
        if (rc != BVSTK_FS_EXISTS) break;
        if (i == 15U) return BVSTK_FS_BUSY;
    }
    if (rc != BVSTK_FS_OK) return rc;
    rc = bvstk_fs_writer_write(&w, data, size);
    bvstk_fs_result_t close = bvstk_fs_writer_close(&w);
    if (rc == BVSTK_FS_OK) rc = close;
    if (rc == BVSTK_FS_OK)
        rc = s->install(s->reader->context, temporary, canonical, backup);
    if (rc != BVSTK_FS_OK) (void)bvstk_fs_remove(s, temporary);
    return rc;
}

const char *bvstk_fs_result_name(bvstk_fs_result_t rc)
{
    switch (rc) {
    case BVSTK_FS_OK: return "OK";
    case BVSTK_FS_INVALID: return "INVALID";
    case BVSTK_FS_RANGE: return "RANGE";
    case BVSTK_FS_NOT_FOUND: return "NOT_FOUND";
    case BVSTK_FS_NOT_READY: return "NOT_READY";
    case BVSTK_FS_TYPE_ERROR: return "TYPE_ERROR";
    case BVSTK_FS_IO: return "IO";
    case BVSTK_FS_CHANGED: return "CHANGED";
    case BVSTK_FS_BUSY: return "BUSY";
    case BVSTK_FS_UNSUPPORTED: return "UNSUPPORTED";
    case BVSTK_FS_EXISTS: return "EXISTS";
    case BVSTK_FS_DENIED: return "DENIED";
    default: return "UNKNOWN";
    }
}
