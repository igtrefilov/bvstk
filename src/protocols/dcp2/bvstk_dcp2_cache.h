#ifndef BVSTK_DCP2_CACHE_H
#define BVSTK_DCP2_CACHE_H
#include "services/cache/bvstk_cache.h"

#define BVSTK_DCP2_CACHE_VERSION 1U
#define BVSTK_DCP2_CACHE_SUBSCRIPTIONS 8U
#define BVSTK_DCP2_CACHE_SNAPSHOT_MS 30000U
#define BVSTK_DCP2_CACHE_PAGE_HEADER 37U
#define BVSTK_DCP2_CACHE_EVENT_HEADER 28U
enum { BVSTK_DCP2_CACHE_GET = 0x20, BVSTK_DCP2_CACHE_SUBSCRIBE = 0x21, BVSTK_DCP2_CACHE_UNSUBSCRIBE = 0x22 };
enum { BVSTK_DCP2_CACHE_NOT_FOUND = 0x0200, BVSTK_DCP2_CACHE_STALE_SNAPSHOT = 0x0201 };
enum { BVSTK_DCP2_CACHE_SNAPSHOT = 0, BVSTK_DCP2_CACHE_ENTRY = 1,
       BVSTK_DCP2_CACHE_PRESENCE = 2, BVSTK_DCP2_CACHE_RESYNC = 3 };

typedef struct {
    uint64_t cursor, revision;
    uint32_t generation;
    uint16_t id, handle;
    uint8_t pending, paused;
} bvstk_dcp2_cache_subscription_t;
typedef struct {
    bvstk_cache_registry_t *registry;
    bvstk_cache_snapshot_t snapshot;
    bvstk_dcp2_cache_subscription_t subscriptions[BVSTK_DCP2_CACHE_SUBSCRIPTIONS];
    uint64_t snapshot_activity;
    uint32_t snapshot_id, next_snapshot;
    uint16_t snapshot_sub, snapshot_progress, next_sub, round_robin;
} bvstk_dcp2_cache_session_t;

void bvstk_dcp2_cache_init(bvstk_dcp2_cache_session_t *, bvstk_cache_registry_t *);
/* Status is encoded by the caller, as with all other DCP2 responses. */
uint16_t bvstk_dcp2_cache_request(bvstk_dcp2_cache_session_t *, uint8_t opcode,
    const uint8_t *body, size_t size, uint64_t now_ms, uint8_t *out, size_t capacity, uint16_t *out_size);
/* At most one event per call; returns encoded bytes, or zero when caught up.
 * Events use service CACHE, opcode SUBSCRIBE | EVENT, sequence zero. */
uint16_t bvstk_dcp2_cache_event(bvstk_dcp2_cache_session_t *, uint64_t now_ms,
    uint8_t *out, size_t capacity);
#endif
