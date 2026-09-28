#include "services/cache/bvstk_cache.h"
#include <string.h>

static void enter(bvstk_cache_registry_t *r) { if (r->guard.enter) r->guard.enter(r->guard.context); }
static void leave(bvstk_cache_registry_t *r) { if (r->guard.leave) r->guard.leave(r->guard.context); }
static int name_valid(const char *s)
{
    size_t n;
    if (!s || !*s) return 0;
    for (n = 0; n < BVSTK_CACHE_NAME_MAX; ++n) {
        unsigned char c = (unsigned char)s[n];
        if (!c) return 1;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return 0;
    }
    return 0;
}
static void normalize(bvstk_cache_entry_t *e)
{
    if (!e->valid) { e->value = 0; e->source = BVSTK_CACHE_SOURCE_UNKNOWN; e->updated_ms = 0; }
    else e->valid = 1;
}
static bvstk_cache_snapshot_t *device(bvstk_cache_registry_t *r, uint16_t handle)
{
    return handle && handle <= BVSTK_CACHE_DEVICES_MAX && r->devices[handle - 1U].generation ?
        &r->devices[handle - 1U] : NULL;
}
static void append(bvstk_cache_registry_t *r, uint16_t handle,
    bvstk_cache_snapshot_t *d, unsigned kind, const bvstk_cache_entry_t *entry, uint64_t time)
{
    bvstk_cache_change_t *c = &r->journal[r->head % BVSTK_CACHE_JOURNAL_SIZE];
    memset(c, 0, sizeof(*c));
    c->serial = ++r->head; c->revision = ++d->revision; c->generation = d->generation;
    c->handle = handle; c->kind = (uint8_t)kind; c->presence = d->presence; c->time_ms = time;
    if (entry) c->entry = *entry;
}
void bvstk_cache_init(bvstk_cache_registry_t *r, const bvstk_cache_guard_t *guard)
{
    memset(r, 0, sizeof(*r));
    if (guard) r->guard = *guard;
}
bvstk_status_t bvstk_cache_define(bvstk_cache_registry_t *r, uint16_t *handle,
    const char *subsystem, const char *name, unsigned count, unsigned presence,
    bvstk_cache_describe_fn describe, void *context)
{
    unsigned i, j, slot;
    bvstk_cache_snapshot_t *d;
    bvstk_status_t result = BVSTK_OK;
    if (!r || !handle || !name_valid(subsystem) || !name_valid(name) || !describe ||
        count > BVSTK_CACHE_RESOURCES_MAX || presence > BVSTK_CACHE_PRESENCE_UNAVAILABLE)
        return BVSTK_ERR_MALFORMED;
    enter(r);
    slot = *handle ? (unsigned)*handle - 1U : BVSTK_CACHE_DEVICES_MAX;
    if (*handle && !device(r, *handle)) { result = BVSTK_ERR_NOT_FOUND; goto done; }
    for (i = 0; i < BVSTK_CACHE_DEVICES_MAX; ++i) {
        d = &r->devices[i];
        if (!d->generation && slot == BVSTK_CACHE_DEVICES_MAX) slot = i;
        if (d->generation && i + 1U != *handle && !strcmp(d->subsystem, subsystem) &&
            !strcmp(d->device, name)) { result = BVSTK_ERR_DENIED; goto done; }
    }
    if (slot == BVSTK_CACHE_DEVICES_MAX) { result = BVSTK_ERR_BUSY; goto done; }
    /* Stage outside the worker stack; publish the complete catalog atomically. */
    memset(&r->staging, 0, sizeof(r->staging));
    for (i = 0; i < count; ++i) {
        bvstk_cache_entry_t *a = &r->staging.entries[i];
        describe(context, i, a);
        if (!name_valid(a->id) || !a->width_bits || a->width_bits > 64 ||
            a->source > BVSTK_CACHE_SOURCE_DEVICE ||
            (a->valid && a->width_bits < 64 && (a->value >> a->width_bits))) {
            result = BVSTK_ERR_MALFORMED; goto done;
        }
        for (j = 0; j < i; ++j) {
            if (!strcmp(a->id, r->staging.entries[j].id)) { result = BVSTK_ERR_MALFORMED; goto done; }
        }
        normalize(a);
    }
    d = &r->devices[slot];
    *d = r->staging;
    strcpy(d->subsystem, subsystem); strcpy(d->device, name);
    if (++r->generation == 0) ++r->generation;
    d->generation = r->generation; d->count = (uint16_t)count; d->presence = (uint8_t)presence;
    *handle = (uint16_t)(slot + 1U);
done:
    leave(r);
    return result;
}
bvstk_status_t bvstk_cache_put(bvstk_cache_registry_t *r, uint16_t handle, unsigned index,
    uint64_t value, int valid, unsigned source, uint64_t updated_ms, uint64_t now_ms)
{
    bvstk_cache_snapshot_t *d;
    bvstk_cache_entry_t *e;
    bvstk_status_t result = BVSTK_OK;
    enter(r); d = device(r, handle);
    if (!d) { result = BVSTK_ERR_NOT_FOUND; goto done; }
    if (index >= d->count || source > BVSTK_CACHE_SOURCE_DEVICE ||
        (valid && d->entries[index].width_bits < 64 && (value >> d->entries[index].width_bits))) {
        result = BVSTK_ERR_RANGE; goto done;
    }
    e = &d->entries[index];
    /* Repeated invalidation has no new information. Repeated valid reads do. */
    if (!valid && !e->valid) goto done;
    e->value = value; e->valid = valid != 0; e->source = (uint8_t)source; e->updated_ms = updated_ms;
    normalize(e); append(r, handle, d, BVSTK_CACHE_CHANGE, e, now_ms);
done:
    leave(r);
    return result;
}
bvstk_status_t bvstk_cache_presence(bvstk_cache_registry_t *r, uint16_t handle,
    unsigned presence, uint64_t now_ms)
{
    bvstk_cache_snapshot_t *d;
    bvstk_status_t result = BVSTK_OK;
    enter(r); d = device(r, handle);
    if (!d) result = BVSTK_ERR_NOT_FOUND;
    else if (presence > BVSTK_CACHE_PRESENCE_UNAVAILABLE) result = BVSTK_ERR_RANGE;
    else if (d->presence != presence) {
        d->presence = (uint8_t)presence; append(r, handle, d, BVSTK_CACHE_PRESENCE, NULL, now_ms);
    }
    leave(r); return result;
}
bvstk_status_t bvstk_cache_snapshot(bvstk_cache_registry_t *r, const char *subsystem,
    const char *name, bvstk_cache_snapshot_t *out, uint16_t *handle, uint64_t *cursor, uint64_t now_ms)
{
    unsigned i;
    bvstk_status_t result = BVSTK_ERR_NOT_FOUND;
    if (!r || !subsystem || !name || !out || !handle || !cursor) return BVSTK_ERR_MALFORMED;
    enter(r);
    for (i = 0; i < BVSTK_CACHE_DEVICES_MAX; ++i) {
        bvstk_cache_snapshot_t *d = &r->devices[i];
        if (d->generation && !strcmp(subsystem, d->subsystem) && !strcmp(name, d->device)) {
            *out = *d; out->captured_ms = now_ms; *cursor = r->head; *handle = (uint16_t)(i + 1U);
            result = BVSTK_OK; break;
        }
    }
    leave(r); return result;
}
int bvstk_cache_next(bvstk_cache_registry_t *r, uint16_t handle, uint32_t generation,
    uint64_t *cursor, bvstk_cache_change_t *out)
{
    bvstk_cache_snapshot_t *d;
    int result = 0;
    enter(r); d = device(r, handle);
    if (!d || d->generation != generation) result = -BVSTK_CACHE_RECONFIGURED;
    else if (*cursor > r->head || r->head - *cursor > BVSTK_CACHE_JOURNAL_SIZE) result = -BVSTK_CACHE_LOST;
    else while (*cursor < r->head) {
        const bvstk_cache_change_t *c = &r->journal[*cursor % BVSTK_CACHE_JOURNAL_SIZE];
        ++*cursor;
        if (c->handle == handle) { *out = *c; result = 1; break; }
    }
    leave(r); return result;
}
