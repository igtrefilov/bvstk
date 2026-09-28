#ifndef BVSTK_CACHE_SINK_H
#define BVSTK_CACHE_SINK_H

#include <stddef.h>
#include <stdint.h>

/* Optional synchronous cache observer. Callbacks must not block or do I/O.
 * enter/leave protect the local mutation and its notification as one operation.
 * The platform supplies these guards before starting service workers. */
typedef struct {
    void *context;
    void (*enter)(void *);
    void (*leave)(void *);
    void (*entry)(void *, size_t device, unsigned resource, uint64_t value,
                  int valid, int seeded, uint64_t updated_ms);
    void (*presence)(void *, size_t device, unsigned presence);
} bvstk_cache_sink_t;

static inline void bvstk_cache_sink_enter(const bvstk_cache_sink_t *s)
{
    if (s->enter) s->enter(s->context);
}
static inline void bvstk_cache_sink_leave(const bvstk_cache_sink_t *s)
{
    if (s->leave) s->leave(s->context);
}
static inline void bvstk_cache_sink_entry(const bvstk_cache_sink_t *s,
    size_t device, unsigned resource, uint64_t value, int valid, int seeded, uint64_t time)
{
    if (s->entry) s->entry(s->context, device, resource, value, valid, seeded, time);
}
static inline void bvstk_cache_sink_presence(const bvstk_cache_sink_t *s,
    size_t device, unsigned presence)
{
    if (s->presence) s->presence(s->context, device, presence);
}
#endif
