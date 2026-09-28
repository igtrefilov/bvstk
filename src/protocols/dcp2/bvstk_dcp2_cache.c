#include "protocols/dcp2/bvstk_dcp2_cache.h"
#include "protocols/dcp2/bvstk_dcp2_codec.h"
#include <string.h>

#define put16 bvstk_dcp2_write_be16
#define put32 bvstk_dcp2_write_be32
#define get16 bvstk_dcp2_read_be16
#define get32 bvstk_dcp2_read_be32

static void put64(uint8_t *p, uint64_t v) { put32(p, (uint32_t)(v >> 32)); put32(p + 4, (uint32_t)v); }
static int name(const uint8_t **p, size_t *n, char *out)
{
    unsigned len, i;
    if (!*n) return 0;
    len = *(*p)++; --*n;
    if (!len || len >= BVSTK_CACHE_NAME_MAX || len > *n || memchr(*p, 0, len)) return 0;
    for (i = 0; i < len; ++i) {
        unsigned c = (*p)[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return 0;
    }
    memcpy(out, *p, len); out[len] = 0; *p += len; *n -= len;
    return 1;
}
static size_t entry(uint8_t *p, const bvstk_cache_entry_t *e)
{
    size_t n = strlen(e->id);
    p[0] = (uint8_t)n; memcpy(p + 1, e->id, n); p += n + 1;
    p[0] = e->width_bits; p[1] = e->valid; p[2] = e->source;
    put64(p + 3, e->value); put64(p + 11, e->updated_ms);
    return n + 20U;
}
static bvstk_dcp2_cache_subscription_t *subscription(bvstk_dcp2_cache_session_t *s, uint16_t id)
{
    unsigned i;
    if (!id) return NULL;
    for (i = 0; i < BVSTK_DCP2_CACHE_SUBSCRIPTIONS; ++i)
        if (s->subscriptions[i].id == id) return &s->subscriptions[i];
    return NULL;
}
static void expire(bvstk_dcp2_cache_session_t *s, uint64_t now)
{
    if (s->snapshot_id && now - s->snapshot_activity >= BVSTK_DCP2_CACHE_SNAPSHOT_MS) {
        bvstk_dcp2_cache_subscription_t *sub = subscription(s, s->snapshot_sub);
        /* Leave pending set until one explicit EXPIRED event has been sent. */
        if (sub && sub->pending) sub->paused = BVSTK_CACHE_EXPIRED;
        s->snapshot_id = 0;
    }
}
void bvstk_dcp2_cache_init(bvstk_dcp2_cache_session_t *s, bvstk_cache_registry_t *registry)
{
    memset(s, 0, sizeof(*s)); s->registry = registry;
}
static uint16_t page(bvstk_dcp2_cache_session_t *s, unsigned first, uint8_t *out,
    size_t capacity, uint16_t *size)
{
    const bvstk_cache_snapshot_t *v = &s->snapshot;
    unsigned count = 0;
    size_t used = BVSTK_DCP2_CACHE_PAGE_HEADER;
    bvstk_dcp2_cache_subscription_t *sub;
    if (first > v->count) return 6;
    if (capacity < used) return 6;
    while (first + count < v->count) {
        const bvstk_cache_entry_t *e = &v->entries[first + count];
        if (strlen(e->id) + 20U > capacity - used) break;
        used += entry(out + used, e); ++count;
    }
    if (!count && first < v->count) return 6;
    out[0] = BVSTK_DCP2_CACHE_VERSION; out[1] = BVSTK_DCP2_CACHE_SNAPSHOT;
    put16(out + 2, s->snapshot_sub); put32(out + 4, s->snapshot_id);
    put32(out + 8, v->generation); put64(out + 12, v->revision); put64(out + 20, v->captured_ms);
    put16(out + 28, v->count); put16(out + 30, (uint16_t)first); put16(out + 32, (uint16_t)count);
    put16(out + 34, first + count == v->count ? 0xFFFFU : (uint16_t)(first + count));
    out[36] = v->presence;
    sub = subscription(s, s->snapshot_sub);
    if (first == s->snapshot_progress) s->snapshot_progress = (uint16_t)(first + count);
    if (sub && s->snapshot_progress == v->count) sub->pending = 0;
    *size = (uint16_t)used;
    return 0;
}
uint16_t bvstk_dcp2_cache_request(bvstk_dcp2_cache_session_t *s, uint8_t opcode,
    const uint8_t *body, size_t size, uint64_t now, uint8_t *out, size_t capacity, uint16_t *out_size)
{
    char bus[BVSTK_CACHE_NAME_MAX], device[BVSTK_CACHE_NAME_MAX];
    uint32_t token = 0;
    uint16_t first = 0, handle;
    uint64_t cursor;
    unsigned i;
    bvstk_status_t st;
    bvstk_dcp2_cache_subscription_t *sub = NULL, *pending;
    *out_size = 0; expire(s, now);
    if (opcode != BVSTK_DCP2_CACHE_GET && opcode != BVSTK_DCP2_CACHE_SUBSCRIBE &&
        opcode != BVSTK_DCP2_CACHE_UNSUBSCRIBE) return 2;
    if (!size) return 1;
    if (*body++ != BVSTK_DCP2_CACHE_VERSION) return 2;
    --size;
    if (opcode == BVSTK_DCP2_CACHE_UNSUBSCRIBE) {
        if (size != 2) return 1;
        sub = subscription(s, get16(body));
        if (!sub) return BVSTK_DCP2_CACHE_NOT_FOUND;
        if (s->snapshot_sub == sub->id) { s->snapshot_sub = 0; s->snapshot_id = 0; }
        memset(sub, 0, sizeof(*sub));
        return 0;
    }
    if (!name(&body, &size, bus) || !name(&body, &size, device)) return 1;
    if (opcode == BVSTK_DCP2_CACHE_GET) {
        if (size != 6) return 1;
        token = get32(body); first = get16(body + 4);
        if (!token && first) return 1;
    } else if (size) return 1;
    if (token) {
        if (token != s->snapshot_id || strcmp(bus, s->snapshot.subsystem) || strcmp(device, s->snapshot.device))
            return BVSTK_DCP2_CACHE_STALE_SNAPSHOT;
        st = (bvstk_status_t)page(s, first, out, capacity, out_size);
        if (st == BVSTK_OK) s->snapshot_activity = now;
        return (uint16_t)st;
    }
    pending = subscription(s, s->snapshot_sub);
    if (s->snapshot_id && pending && pending->pending) return 4;
    /* Guarantee at least one maximal entry before creating subscription state. */
    if (capacity < BVSTK_DCP2_CACHE_PAGE_HEADER + BVSTK_CACHE_NAME_MAX - 1U + 20U) return 6;
    if (opcode == BVSTK_DCP2_CACHE_SUBSCRIBE) {
        for (i = 0; i < BVSTK_DCP2_CACHE_SUBSCRIPTIONS; ++i) {
            bvstk_dcp2_cache_subscription_t *candidate = &s->subscriptions[i];
            if (!candidate->id && !sub) sub = candidate;
            /* Same target is replaced below after its handle is resolved. */
        }
        /* Capture first even when full: the target may already be subscribed. */
    }
    st = bvstk_cache_snapshot(s->registry, bus, device, &s->snapshot, &handle, &cursor, now);
    if (st != BVSTK_OK) return BVSTK_DCP2_CACHE_NOT_FOUND;
    /* Any new capture replaces the old token, including failed subscriptions. */
    s->snapshot_id = 0; s->snapshot_sub = 0; s->snapshot_progress = 0;
    if (opcode == BVSTK_DCP2_CACHE_SUBSCRIBE) {
        for (i = 0; i < BVSTK_DCP2_CACHE_SUBSCRIPTIONS; ++i)
            if (s->subscriptions[i].id && s->subscriptions[i].handle == handle) { sub = &s->subscriptions[i]; break; }
        if (!sub || (!sub->id && s->next_sub == UINT16_MAX)) return 4;
        if (!sub->id) sub->id = ++s->next_sub;
        sub->handle = handle; sub->generation = s->snapshot.generation;
        sub->revision = s->snapshot.revision; sub->cursor = cursor;
        sub->pending = 1; sub->paused = 0; s->snapshot_sub = sub->id;
    }
    if (++s->next_snapshot == 0) ++s->next_snapshot;
    s->snapshot_id = s->next_snapshot; s->snapshot_activity = now;
    return page(s, 0, out, capacity, out_size);
}
uint16_t bvstk_dcp2_cache_event(bvstk_dcp2_cache_session_t *s, uint64_t now, uint8_t *out, size_t capacity)
{
    unsigned i;
    bvstk_cache_change_t change;
    expire(s, now);
    if (capacity < BVSTK_DCP2_CACHE_EVENT_HEADER + BVSTK_CACHE_NAME_MAX - 1U + 20U) return 0;
    for (i = 0; i < BVSTK_DCP2_CACHE_SUBSCRIPTIONS; ++i) {
        bvstk_dcp2_cache_subscription_t *sub = &s->subscriptions[s->round_robin++ % BVSTK_DCP2_CACHE_SUBSCRIPTIONS];
        int result;
        size_t used = BVSTK_DCP2_CACHE_EVENT_HEADER;
        if (!sub->id || sub->paused == 0xFFU) continue;
        if (sub->paused) result = -(int)sub->paused;
        else if (sub->pending) continue;
        else result = bvstk_cache_next(s->registry, sub->handle, sub->generation, &sub->cursor, &change);
        if (!result) continue;
        if (result > 0 && change.revision != sub->revision + 1U) result = -BVSTK_CACHE_LOST;
        memset(out, 0, used); out[0] = BVSTK_DCP2_CACHE_VERSION;
        put16(out + 2, sub->id); put32(out + 4, sub->generation);
        if (result < 0) {
            out[1] = BVSTK_DCP2_CACHE_RESYNC; out[25] = (uint8_t)-result;
            put64(out + 8, sub->revision); put64(out + 16, now);
            sub->pending = 0; sub->paused = 0xFFU;
        } else {
            out[1] = change.kind; put64(out + 8, change.revision); put64(out + 16, change.time_ms);
            out[24] = change.presence; sub->revision = change.revision;
            if (change.kind == BVSTK_CACHE_CHANGE) {
                put16(out + 26, 1); used += entry(out + used, &change.entry);
            }
        }
        return (uint16_t)used;
    }
    return 0;
}
