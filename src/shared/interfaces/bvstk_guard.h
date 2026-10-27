#ifndef BVSTK_GUARD_H
#define BVSTK_GUARD_H

/* Short memory-only sections. A guard must support nesting when a cache sink
 * publishes to the registry under the same guard. Never perform device,
 * filesystem or network I/O while holding it. */
typedef struct {
    void *context;
    void (*enter)(void *);
    void (*leave)(void *);
} bvstk_guard_t;

#endif
