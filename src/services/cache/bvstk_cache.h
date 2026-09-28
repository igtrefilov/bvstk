#ifndef BVSTK_CACHE_H
#define BVSTK_CACHE_H

#include "shared/base/bvstk_status.h"
#include <stddef.h>
#include <stdint.h>

/* Implementation bounds, independent of subsystem names and address formats. */
#define BVSTK_CACHE_DEVICES_MAX 48U
#define BVSTK_CACHE_RESOURCES_MAX 256U
#define BVSTK_CACHE_NAME_MAX 32U
#define BVSTK_CACHE_JOURNAL_SIZE 512U

enum { BVSTK_CACHE_SOURCE_UNKNOWN, BVSTK_CACHE_SOURCE_SETTINGS, BVSTK_CACHE_SOURCE_DEVICE };
enum { BVSTK_CACHE_PRESENCE_UNKNOWN, BVSTK_CACHE_PRESENCE_READY, BVSTK_CACHE_PRESENCE_UNAVAILABLE };
enum { BVSTK_CACHE_CHANGE = 1, BVSTK_CACHE_PRESENCE = 2 };
enum { BVSTK_CACHE_LOST = 1, BVSTK_CACHE_RECONFIGURED = 2, BVSTK_CACHE_EXPIRED = 3 };

typedef struct {
    char id[BVSTK_CACHE_NAME_MAX];
    uint64_t value, updated_ms;
    uint8_t width_bits, valid, source;
} bvstk_cache_entry_t;

typedef struct {
    char subsystem[BVSTK_CACHE_NAME_MAX], device[BVSTK_CACHE_NAME_MAX];
    uint32_t generation;
    uint64_t revision, captured_ms;
    uint16_t count;
    uint8_t presence;
    bvstk_cache_entry_t entries[BVSTK_CACHE_RESOURCES_MAX];
} bvstk_cache_snapshot_t;

typedef struct {
    uint64_t serial, revision, time_ms;
    uint32_t generation;
    uint16_t handle;
    uint8_t kind, presence;
    bvstk_cache_entry_t entry;
} bvstk_cache_change_t;

typedef struct {
    void *context;
    void (*enter)(void *);
    void (*leave)(void *);
} bvstk_cache_guard_t;

typedef struct {
    bvstk_cache_guard_t guard;
    uint32_t generation;
    uint64_t head;
    bvstk_cache_snapshot_t devices[BVSTK_CACHE_DEVICES_MAX];
    bvstk_cache_snapshot_t staging;
    bvstk_cache_change_t journal[BVSTK_CACHE_JOURNAL_SIZE];
} bvstk_cache_registry_t;

/* Describes a resource and its initial cached state without hardware access.
 * Called synchronously under the registry guard; must not block or do I/O. */
typedef void (*bvstk_cache_describe_fn)(void *, unsigned index, bvstk_cache_entry_t *);

void bvstk_cache_init(bvstk_cache_registry_t *, const bvstk_cache_guard_t *);
/* *handle == 0 registers a new device; otherwise replaces that device's catalog.
 * Replacement changes generation and forces existing subscribers to resync.
 * A failed definition leaves the previous catalog intact. */
bvstk_status_t bvstk_cache_define(bvstk_cache_registry_t *, uint16_t *handle,
    const char *subsystem, const char *device, unsigned count, unsigned presence,
    bvstk_cache_describe_fn, void *context);
bvstk_status_t bvstk_cache_put(bvstk_cache_registry_t *, uint16_t handle, unsigned index,
    uint64_t value, int valid, unsigned source, uint64_t updated_ms, uint64_t now_ms);
bvstk_status_t bvstk_cache_presence(bvstk_cache_registry_t *, uint16_t handle,
    unsigned presence, uint64_t now_ms);
bvstk_status_t bvstk_cache_snapshot(bvstk_cache_registry_t *, const char *subsystem,
    const char *device, bvstk_cache_snapshot_t *, uint16_t *handle, uint64_t *cursor,
    uint64_t now_ms);
/* 1 = change, 0 = caught up, -reason = resynchronization required.
 * cursor is an opaque journal position obtained atomically with the snapshot. */
int bvstk_cache_next(bvstk_cache_registry_t *, uint16_t handle, uint32_t generation,
    uint64_t *cursor, bvstk_cache_change_t *);
#endif
