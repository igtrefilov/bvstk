#include "protocols/dcp2/bvstk_dcp2_fs.h"

#include <string.h>
#include "protocols/dcp2/bvstk_dcp2_codec.h"

enum { OK = 0, MALFORMED = 1, UNSUPPORTED = 2, BUSY = 4, RANGE = 6, INTERNAL = 7 };

static void put64(uint8_t *out, uint64_t value)
{
    bvstk_dcp2_write_be32(out, (uint32_t)(value >> 32));
    bvstk_dcp2_write_be32(out + 4, (uint32_t)value);
}

static uint16_t wire_status(bvstk_fs_result_t result)
{
    switch (result) {
    case BVSTK_FS_OK: return OK;
    case BVSTK_FS_INVALID: return MALFORMED;
    case BVSTK_FS_RANGE: return RANGE;
    case BVSTK_FS_NOT_FOUND: return BVSTK_DCP2_FS_NOT_FOUND;
    case BVSTK_FS_NOT_READY: return BVSTK_DCP2_FS_NOT_READY;
    case BVSTK_FS_TYPE_ERROR: return BVSTK_DCP2_FS_TYPE_ERROR;
    case BVSTK_FS_IO: return BVSTK_DCP2_FS_IO_ERROR;
    case BVSTK_FS_CHANGED: return BVSTK_DCP2_FS_CHANGED;
    case BVSTK_FS_BUSY: return BUSY;
    case BVSTK_FS_UNSUPPORTED: return UNSUPPORTED;
    default: return INTERNAL;
    }
}

static void close_handle(bvstk_dcp2_fs_handle_t *h)
{
    if (h->handle) bvstk_fs_reader_close(&h->reader);
    memset(h, 0, sizeof(*h));
}

void bvstk_dcp2_fs_init(bvstk_dcp2_fs_session_t *s, const bvstk_fs_backend_t *backend)
{
    memset(s, 0, sizeof(*s));
    s->backend = backend;
    s->next_handle = 1;
}

void bvstk_dcp2_fs_destroy(bvstk_dcp2_fs_session_t *s)
{
    size_t i;
    for (i = 0; i < BVSTK_DCP2_FS_HANDLES_MAX; ++i) close_handle(&s->handles[i]);
}

void bvstk_dcp2_fs_expire(bvstk_dcp2_fs_session_t *s, uint32_t now)
{
    size_t i;
    for (i = 0; i < BVSTK_DCP2_FS_HANDLES_MAX; ++i) {
        bvstk_dcp2_fs_handle_t *h = &s->handles[i];
        if (h->handle && (uint32_t)(now - h->last_activity) >= BVSTK_DCP2_FS_IDLE_MS) close_handle(h);
    }
}

static uint16_t parse_path(const uint8_t *body, size_t size, char *out)
{
    uint16_t length;
    if (size < 2) return MALFORMED;
    length = bvstk_dcp2_read_be16(body);
    if (size != (size_t)length + 2) return MALFORMED;
    return wire_status(bvstk_fs_validate_path(body + 2, length, out, BVSTK_FS_PATH_CAPACITY));
}

static uint16_t info_response(bvstk_dcp2_fs_session_t *s, uint8_t *out, size_t capacity, uint16_t *size)
{
    size_t count = s->backend->volume_count(s->backend->context), i, offset = 20;
    if (capacity < offset || count > UINT16_MAX) return INTERNAL;
    bvstk_dcp2_write_be16(out, BVSTK_DCP2_FS_VERSION);
    /* FILE, TREE, RECURSIVE, TAR, UTF8 paths, LIVE_READ (no snapshot). */
    bvstk_dcp2_write_be32(out + 2, 0x3FU);
    bvstk_dcp2_write_be16(out + 6, BVSTK_DCP2_FS_BLOCK_MAX);
    bvstk_dcp2_write_be16(out + 8, BVSTK_FS_PATH_CAPACITY - 1);
    bvstk_dcp2_write_be16(out + 10, BVSTK_FS_MAX_DEPTH);
    bvstk_dcp2_write_be16(out + 12, BVSTK_DCP2_FS_HANDLES_MAX);
    bvstk_dcp2_write_be32(out + 14, BVSTK_DCP2_FS_IDLE_MS);
    bvstk_dcp2_write_be16(out + 18, (uint16_t)count);
    for (i = 0; i < count; ++i) {
        bvstk_fs_volume_t volume;
        size_t length;
        uint16_t status = wire_status(s->backend->volume(s->backend->context, i, &volume));
        if (status) return status;
        length = strlen(volume.name);
        if (!length || length >= sizeof(volume.name) || offset + 2 + length > capacity) return INTERNAL;
        out[offset++] = volume.state;
        out[offset++] = (uint8_t)length;
        memcpy(out + offset, volume.name, length);
        offset += length;
    }
    *size = (uint16_t)offset;
    return OK;
}

static uint16_t open_response(bvstk_dcp2_fs_session_t *s, const uint8_t *body, size_t size,
                              uint32_t now, uint8_t *out, size_t capacity, uint16_t *out_size)
{
    char path[BVSTK_FS_PATH_CAPACITY];
    uint32_t open_id;
    uint16_t requested, status;
    bvstk_dcp2_fs_handle_t *slot = NULL;
    size_t i;
    if (size < 10) return MALFORMED;
    if (capacity < 14) return INTERNAL;
    open_id = bvstk_dcp2_read_be32(body);
    requested = bvstk_dcp2_read_be16(body + 6);
    if (!open_id) return MALFORMED;
    if (requested > BVSTK_DCP2_FS_BLOCK_MAX) return RANGE;
    if (body[4] < BVSTK_FS_FILE || body[4] > BVSTK_FS_TAR) return UNSUPPORTED;
    if ((body[4] != BVSTK_FS_TREE && body[5]) || (body[5] & ~BVSTK_FS_RECURSIVE)) return MALFORMED;
    status = parse_path(body + 8, size - 8, path);
    if (status) return status;
    for (i = 0; i < BVSTK_DCP2_FS_HANDLES_MAX; ++i) {
        bvstk_dcp2_fs_handle_t *h = &s->handles[i];
        if (h->handle && h->open_id == open_id) {
            if (h->reader.kind != body[4] || h->reader.flags != body[5] ||
                h->requested_block_size != requested || strcmp(h->reader.root, path)) return MALFORMED;
            slot = h;
            break;
        }
        if (!h->handle && !slot) slot = h;
    }
    if (!slot || !s->next_handle) return BUSY;
    if (!slot->handle) {
        status = wire_status(bvstk_fs_reader_open(&slot->reader, s->backend, path, body[4], body[5]));
        if (status) { memset(slot, 0, sizeof(*slot)); return status; }
        slot->handle = s->next_handle++;
        slot->open_id = open_id;
        slot->requested_block_size = requested;
        slot->block_size = requested ? requested : BVSTK_DCP2_FS_BLOCK_MAX;
    }
    slot->last_activity = now;
    bvstk_dcp2_write_be32(out, slot->handle);
    bvstk_dcp2_write_be16(out + 4, slot->block_size);
    put64(out + 6, slot->reader.total_size);
    *out_size = 14;
    return OK;
}

static uint16_t read_response(bvstk_dcp2_fs_handle_t *h, uint32_t block, uint8_t *out,
                              size_t capacity, uint16_t *out_size)
{
    if (capacity < (size_t)h->block_size + 12) return INTERNAL;
    if (h->terminal_status) return h->terminal_status;
    if (!(h->has_block && block == h->last_block)) {
        size_t size = 0;
        bool eof = false;
        uint16_t status;
        if ((!h->has_block && block != 0) ||
            (h->has_block && (h->eof || h->last_block == UINT32_MAX || block != h->last_block + 1))) return RANGE;
        status = wire_status(bvstk_fs_reader_read(&h->reader, h->cached, h->block_size, &size, &eof));
        if (status) {
            /* A partially generated block cannot be retried after advancing I/O. */
            if (status == BUSY) status = BVSTK_DCP2_FS_IO_ERROR;
            h->terminal_status = status;
            return status;
        }
        h->last_block = block;
        h->has_block = true;
        h->cached_size = (uint16_t)size;
        h->eof = eof;
    }
    bvstk_dcp2_write_be32(out, h->handle);
    bvstk_dcp2_write_be32(out + 4, block);
    out[8] = h->eof ? BVSTK_DCP2_FS_EOF : 0;
    out[9] = 0;
    bvstk_dcp2_write_be16(out + 10, h->cached_size);
    memcpy(out + 12, h->cached, h->cached_size);
    *out_size = (uint16_t)(12 + h->cached_size);
    return OK;
}

uint16_t bvstk_dcp2_fs_request(bvstk_dcp2_fs_session_t *s, uint8_t opcode,
                              const uint8_t *body, size_t size, uint32_t now,
                              uint8_t *out, size_t capacity, uint16_t *out_size)
{
    size_t i;
    uint32_t handle;
    if (!s || !s->backend || !out || !out_size || (!body && size)) return INTERNAL;
    *out_size = 0;
    bvstk_dcp2_fs_expire(s, now);
    if (opcode == BVSTK_DCP2_FS_INFO)
        return size ? MALFORMED : info_response(s, out, capacity, out_size);
    if (opcode == BVSTK_DCP2_FS_STAT) {
        char path[BVSTK_FS_PATH_CAPACITY];
        bvstk_fs_info_t info;
        uint16_t status = parse_path(body, size, path);
        if (status) return status;
        if (capacity < 10) return INTERNAL;
        status = wire_status(s->backend->stat(s->backend->context, path, &info));
        if (status) return status;
        out[0] = info.type;
        out[1] = info.attributes;
        put64(out + 2, info.type == BVSTK_FS_ENTRY_DIR ? 0 : info.size);
        *out_size = 10;
        return OK;
    }
    if (opcode == BVSTK_DCP2_FS_OPEN) return open_response(s, body, size, now, out, capacity, out_size);
    if (opcode != BVSTK_DCP2_FS_READ && opcode != BVSTK_DCP2_FS_CLOSE) return UNSUPPORTED;
    if (size != (opcode == BVSTK_DCP2_FS_READ ? 8U : 4U)) return MALFORMED;
    handle = bvstk_dcp2_read_be32(body);
    if (!handle) return BVSTK_DCP2_FS_BAD_HANDLE;
    for (i = 0; i < BVSTK_DCP2_FS_HANDLES_MAX; ++i) {
        bvstk_dcp2_fs_handle_t *h = &s->handles[i];
        if (h->handle != handle) continue;
        if (opcode == BVSTK_DCP2_FS_CLOSE) { close_handle(h); return OK; }
        h->last_activity = now;
        return read_response(h, bvstk_dcp2_read_be32(body + 4), out, capacity, out_size);
    }
    return opcode == BVSTK_DCP2_FS_CLOSE ? OK : BVSTK_DCP2_FS_BAD_HANDLE;
}
