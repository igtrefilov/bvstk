#ifndef BVSTK_SHARED_CLOCK_H
#define BVSTK_SHARED_CLOCK_H

#include <stddef.h>
#include <stdint.h>

/*
 * The common runtime uses a monotonic millisecond clock.  The implementation
 * is supplied by the target port; no RTOS tick type is allowed to leak into
 * portable drivers and services.
 */
uint64_t bvstk_platform_now_ms(void);
void bvstk_platform_sleep_ms(uint32_t milliseconds);

typedef struct {
    void *context;
    uint64_t (*now_ms)(void *context);
    void (*sleep_ms)(void *context, uint32_t milliseconds);
} bvstk_clock_t;

static inline uint64_t bvstk_clock_now_ms(const bvstk_clock_t *clock)
{
    return (clock != NULL && clock->now_ms != NULL)
               ? clock->now_ms(clock->context)
               : bvstk_platform_now_ms();
}

static inline void bvstk_clock_sleep_ms(const bvstk_clock_t *clock,
                                        uint32_t milliseconds)
{
    if (clock != NULL && clock->sleep_ms != NULL) {
        clock->sleep_ms(clock->context, milliseconds);
    } else {
        bvstk_platform_sleep_ms(milliseconds);
    }
}

#endif /* BVSTK_SHARED_CLOCK_H */
