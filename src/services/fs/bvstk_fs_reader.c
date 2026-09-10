#include "services/fs/bvstk_fs_reader.h"

#include <stdio.h>
#include <string.h>

enum { WALK, FILE_DATA, FILE_PADDING, FINISHED };

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v;
}

static void put64(uint8_t *p, uint64_t v)
{
    size_t i;
    for (i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (56U - 8U * i));
}

bool bvstk_fs_utf8_decode(const uint8_t *data, size_t size, size_t *used, uint32_t *cp)
{
    size_t n, i;
    uint32_t value, minimum;
    if (!data || !size || !used || !cp) return false;
    if (data[0] < 0x80) { *used = 1; *cp = data[0]; return true; }
    if (data[0] >= 0xC2 && data[0] <= 0xDF) { n = 2; value = data[0] & 31U; minimum = 0x80; }
    else if (data[0] >= 0xE0 && data[0] <= 0xEF) { n = 3; value = data[0] & 15U; minimum = 0x800; }
    else if (data[0] >= 0xF0 && data[0] <= 0xF4) { n = 4; value = data[0] & 7U; minimum = 0x10000; }
    else return false;
    if (size < n) return false;
    for (i = 1; i < n; ++i) {
        if ((data[i] & 0xC0) != 0x80) return false;
        value = (value << 6) | (data[i] & 63U);
    }
    if (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) return false;
    *used = n; *cp = value;
    return true;
}

static bool valid_component(const char *name, size_t length)
{
    size_t pos = 0, used;
    uint32_t cp;
    if (!length || (length == 1 && name[0] == '.') ||
        (length == 2 && name[0] == '.' && name[1] == '.')) return false;
    while (pos < length) {
        if (!bvstk_fs_utf8_decode((const uint8_t *)name + pos, length - pos, &used, &cp)) return false;
        if (cp < 32 || cp == 127 || cp == '/' || cp == '\\' || cp == ':' ||
            cp == '*' || cp == '?' || cp == '"' || cp == '<' || cp == '>' || cp == '|') return false;
        pos += used;
    }
    return true;
}

bvstk_fs_result_t bvstk_fs_validate_path(const uint8_t *data, size_t size, char *out, size_t capacity)
{
    size_t colon = 0, start, pos;
    if (!data || !out || !size) return BVSTK_FS_INVALID;
    if (size >= capacity || size >= BVSTK_FS_PATH_CAPACITY) return BVSTK_FS_RANGE;
    while (colon < size && data[colon] != ':') {
        uint8_t c = data[colon];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return BVSTK_FS_INVALID;
        ++colon;
    }
    if (!colon || colon >= 16 || colon + 1 >= size || data[colon + 1] != '/') return BVSTK_FS_INVALID;
    start = colon + 2;
    for (pos = start; pos <= size; ++pos) {
        if (pos == size || data[pos] == '/') {
            if (pos != start) {
                if (!valid_component((const char *)data + start, pos - start)) return BVSTK_FS_INVALID;
            } else if (pos != size) return BVSTK_FS_INVALID;
            start = pos + 1;
        }
    }
    memcpy(out, data, size);
    /* Canonicalize a directory's trailing separator, retaining the volume root. */
    if (size > colon + 2 && out[size - 1] == '/') --size;
    out[size] = '\0';
    return BVSTK_FS_OK;
}

void bvstk_fs_reader_close(bvstk_fs_reader_t *r)
{
    if (!r || !r->backend) return;
    if (r->file) r->backend->file_close(r->backend->context, r->file);
    r->file = NULL;
    while (r->depth) {
        --r->depth;
        r->backend->dir_close(r->backend->context, r->dirs[r->depth].handle);
        r->dirs[r->depth].handle = NULL;
    }
    r->phase = FINISHED;
}

static bvstk_fs_result_t open_file(bvstk_fs_reader_t *r)
{
    uint64_t size = 0;
    bvstk_fs_result_t rc = r->backend->file_open(r->backend->context, r->path, &r->file, &size);
    if (rc != BVSTK_FS_OK) return rc;
    if (size != r->info.size) return BVSTK_FS_CHANGED;
    r->remaining = size;
    return BVSTK_FS_OK;
}

bvstk_fs_result_t bvstk_fs_reader_open(bvstk_fs_reader_t *r, const bvstk_fs_backend_t *backend,
                                      const char *path, uint8_t kind, uint8_t flags)
{
    bvstk_fs_result_t rc;
    size_t length;
    if (!r || !backend || !path) return BVSTK_FS_INVALID;
    memset(r, 0, sizeof(*r));
    r->backend = backend;
    r->kind = kind; r->flags = flags;
    length = strlen(path);
    if (length >= sizeof(r->root)) return BVSTK_FS_RANGE;
    if (kind < BVSTK_FS_FILE || kind > BVSTK_FS_TAR) return BVSTK_FS_UNSUPPORTED;
    if ((kind != BVSTK_FS_TREE && flags) || (flags & ~BVSTK_FS_RECURSIVE)) return BVSTK_FS_INVALID;
    memcpy(r->root, path, length + 1);
    memcpy(r->path, path, length + 1);
    r->relative_offset = length + (length && path[length - 1] != '/' ? 1U : 0U);
    rc = backend->stat(backend->context, path, &r->info);
    if (rc != BVSTK_FS_OK) return rc;
    if (r->info.type != (kind == BVSTK_FS_FILE ? BVSTK_FS_ENTRY_FILE : BVSTK_FS_ENTRY_DIR)) return BVSTK_FS_TYPE_ERROR;
    r->total_size = kind == BVSTK_FS_FILE ? r->info.size : UINT64_MAX;
    if (kind == BVSTK_FS_FILE) {
        rc = open_file(r);
        r->phase = FILE_DATA;
    } else {
        rc = backend->dir_open(backend->context, path, &r->dirs[0].handle);
        if (rc == BVSTK_FS_OK) { r->depth = 1; r->dirs[0].path_len = length; }
        r->phase = WALK;
    }
    if (rc != BVSTK_FS_OK) bvstk_fs_reader_close(r);
    return rc;
}

static bvstk_fs_result_t next_entry(bvstk_fs_reader_t *r, bool *done)
{
    char name[BVSTK_FS_PATH_CAPACITY];
    const bvstk_fs_backend_t *b = r->backend;
    *done = false;
    while (r->depth) {
        size_t top = r->depth - 1, parent = r->dirs[top].path_len, length;
        bvstk_fs_result_t rc;
        r->path[parent] = '\0';
        name[0] = '\0';
        rc = b->dir_read(b->context, r->dirs[top].handle, name, sizeof(name), &r->info);
        if (rc != BVSTK_FS_OK) return rc;
        if (!name[0]) {
            b->dir_close(b->context, r->dirs[top].handle);
            r->dirs[top].handle = NULL;
            --r->depth;
            continue;
        }
        length = strlen(name);
        if (!valid_component(name, length)) return BVSTK_FS_INVALID;
        if (parent && r->path[parent - 1] != '/') {
            if (parent + 1 >= sizeof(r->path)) return BVSTK_FS_RANGE;
            r->path[parent++] = '/';
        }
        if (parent + length >= sizeof(r->path)) return BVSTK_FS_RANGE;
        memcpy(r->path + parent, name, length + 1);
        if (r->info.type == BVSTK_FS_ENTRY_DIR &&
            (r->kind == BVSTK_FS_TAR || (r->flags & BVSTK_FS_RECURSIVE))) {
            if (r->depth == BVSTK_FS_MAX_DEPTH) return BVSTK_FS_RANGE;
            rc = b->dir_open(b->context, r->path, &r->dirs[r->depth].handle);
            if (rc != BVSTK_FS_OK) return rc;
            r->dirs[r->depth].path_len = parent + length;
            ++r->depth;
        } else if (r->info.type != BVSTK_FS_ENTRY_FILE && r->info.type != BVSTK_FS_ENTRY_DIR) {
            return BVSTK_FS_UNSUPPORTED;
        }
        return BVSTK_FS_OK;
    }
    *done = true;
    return BVSTK_FS_OK;
}

static void tar_header(uint8_t *header, const char *name, char type, uint64_t size)
{
    unsigned checksum = 0;
    size_t i;
    memset(header, 0, 512);
    memcpy(header, name, strlen(name));
    memcpy(header + 100, type == '5' ? "0000755" : "0000644", 7);
    memcpy(header + 108, "0000000", 7);
    memcpy(header + 116, "0000000", 7);
    (void)snprintf((char *)header + 124, 12, "%011llo", (unsigned long long)size);
    memcpy(header + 136, "00000000000", 11);
    memset(header + 148, ' ', 8);
    header[156] = (uint8_t)type;
    memcpy(header + 257, "ustar", 5);
    memcpy(header + 263, "00", 2);
    for (i = 0; i < 512; ++i) checksum += header[i];
    (void)snprintf((char *)header + 148, 7, "%06o", checksum);
    header[155] = ' ';
}

static size_t pax_record(char *out, size_t capacity, const char *key, const char *value)
{
    size_t base = strlen(key) + strlen(value) + 3, length = base + 1, next;
    int digits, result;
    do {
        digits = snprintf(NULL, 0, "%lu", (unsigned long)length);
        next = base + (size_t)digits;
        if (next == length) break;
        length = next;
    } while (true);
    result = snprintf(out, capacity, "%lu %s=%s\n", (unsigned long)length, key, value);
    return result >= 0 && (size_t)result < capacity ? (size_t)result : 0;
}

static bvstk_fs_result_t stage_tar_entry(bvstk_fs_reader_t *r)
{
    char name[BVSTK_FS_PATH_CAPACITY + 1];
    size_t length, i, pax_size = 0, offset = 0;
    bool extended;
    uint64_t size = r->info.type == BVSTK_FS_ENTRY_FILE ? r->info.size : 0;
    const uint64_t octal_max = UINT64_C(077777777777);
    (void)snprintf(name, sizeof(name), "%s%s", r->path + r->relative_offset,
                   r->info.type == BVSTK_FS_ENTRY_DIR ? "/" : "");
    length = strlen(name);
    extended = length >= 100 || size > octal_max;
    for (i = 0; i < length; ++i) if ((uint8_t)name[i] >= 128) extended = true;
    memset(r->stage, 0, sizeof(r->stage));
    if (extended) {
        size_t n = pax_record((char *)r->stage + 512, 1024, "path", name);
        if (!n) return BVSTK_FS_RANGE;
        pax_size = n;
        if (size > octal_max) {
            char value[32];
            (void)snprintf(value, sizeof(value), "%llu", (unsigned long long)size);
            n = pax_record((char *)r->stage + 512 + pax_size, 1024 - pax_size, "size", value);
            if (!n) return BVSTK_FS_RANGE;
            pax_size += n;
        }
        tar_header(r->stage, "PaxHeader", 'x', pax_size);
        offset = 512 + ((pax_size + 511) / 512) * 512;
    }
    tar_header(r->stage + offset, extended ? "entry" : name,
               r->info.type == BVSTK_FS_ENTRY_DIR ? '5' : '0', size > octal_max ? 0 : size);
    r->stage_size = offset + 512;
    r->stage_offset = 0;
    return BVSTK_FS_OK;
}

static bvstk_fs_result_t finish_file(bvstk_fs_reader_t *r)
{
    bvstk_fs_info_t current;
    bvstk_fs_result_t rc = r->backend->stat(r->backend->context, r->path, &current);
    r->backend->file_close(r->backend->context, r->file);
    r->file = NULL;
    if (rc == BVSTK_FS_NOT_FOUND) return BVSTK_FS_CHANGED;
    if (rc != BVSTK_FS_OK) return rc;
    if (current.type != BVSTK_FS_ENTRY_FILE || current.size != r->info.size || current.stamp != r->info.stamp)
        return BVSTK_FS_CHANGED;
    if (r->kind == BVSTK_FS_FILE) r->phase = FINISHED;
    else {
        r->padding = (size_t)((512 - (r->info.size % 512)) % 512);
        r->phase = FILE_PADDING;
    }
    return BVSTK_FS_OK;
}

bvstk_fs_result_t bvstk_fs_reader_read(bvstk_fs_reader_t *r, uint8_t *data, size_t capacity,
                                      size_t *size, bool *eof)
{
    bvstk_fs_result_t rc = BVSTK_FS_OK;
    if (!r || !r->backend || !data || !capacity || !size || !eof) return BVSTK_FS_INVALID;
    *size = 0; *eof = false;
    while (*size < capacity) {
        size_t available = capacity - *size;
        if (r->stage_offset < r->stage_size) {
            size_t n = r->stage_size - r->stage_offset;
            if (n > available) n = available;
            memcpy(data + *size, r->stage + r->stage_offset, n);
            r->stage_offset += n; *size += n;
        } else if (r->phase == FILE_DATA) {
            size_t n = 0, want = available;
            if (!r->remaining) {
                rc = finish_file(r);
                if (rc != BVSTK_FS_OK) break;
                continue;
            }
            if (r->remaining < want) want = (size_t)r->remaining;
            rc = r->backend->file_read(r->backend->context, r->file, data + *size, want, &n);
            if (rc != BVSTK_FS_OK) break;
            if (!n || n > want) { rc = BVSTK_FS_CHANGED; break; }
            *size += n; r->remaining -= n;
        } else if (r->phase == FILE_PADDING) {
            size_t n = r->padding < available ? r->padding : available;
            memset(data + *size, 0, n);
            *size += n; r->padding -= n;
            if (!r->padding) r->phase = WALK;
        } else if (r->phase == WALK) {
            bool done;
            rc = next_entry(r, &done);
            if (rc != BVSTK_FS_OK) break;
            r->stage_offset = 0;
            if (done) {
                r->phase = FINISHED;
                r->stage_size = r->kind == BVSTK_FS_TAR ? 1024 : 0;
                memset(r->stage, 0, r->stage_size);
            } else if (r->kind == BVSTK_FS_TREE) {
                const char *relative = r->path + r->relative_offset;
                size_t length = strlen(relative);
                r->stage_size = 14 + length;
                put16(r->stage, (uint16_t)r->stage_size);
                r->stage[2] = r->info.type;
                r->stage[3] = 0;
                put64(r->stage + 4, r->info.type == BVSTK_FS_ENTRY_FILE ? r->info.size : 0);
                put16(r->stage + 12, (uint16_t)length);
                memcpy(r->stage + 14, relative, length);
            } else {
                if (r->info.type == BVSTK_FS_ENTRY_FILE) {
                    rc = open_file(r);
                    if (rc != BVSTK_FS_OK) break;
                    r->phase = FILE_DATA;
                }
                rc = stage_tar_entry(r);
                if (rc != BVSTK_FS_OK) break;
            }
        } else break;
    }
    /* Finish a file even when its last byte exactly fills the output block. */
    if (rc == BVSTK_FS_OK && r->phase == FILE_DATA && !r->remaining && r->stage_offset == r->stage_size)
        rc = finish_file(r);
    if (rc != BVSTK_FS_OK) { bvstk_fs_reader_close(r); return rc; }
    *eof = r->phase == FINISHED && r->stage_offset == r->stage_size;
    return BVSTK_FS_OK;
}
