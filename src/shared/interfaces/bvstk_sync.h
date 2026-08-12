#ifndef BVSTK_SHARED_SYNC_H
#define BVSTK_SHARED_SYNC_H

#include <stdint.h>

#include "shared/base/bvstk_status.h"

/*
 * A deliberately small mutex contract.  FreeRTOS and Neutrino adapters own
 * the concrete object; portable code only sees this callback table.
 */
typedef struct {
    void *context;
    bvstk_status_t (*lock)(void *context, uint32_t timeout_ms);
    void (*unlock)(void *context);
} bvstk_mutex_t;

static inline bvstk_status_t bvstk_mutex_lock(const bvstk_mutex_t *mutex,
                                              uint32_t timeout_ms)
{
    if (mutex == NULL || mutex->lock == NULL) {
        return BVSTK_OK;
    }
    return mutex->lock(mutex->context, timeout_ms);
}

static inline void bvstk_mutex_unlock(const bvstk_mutex_t *mutex)
{
    if (mutex != NULL && mutex->unlock != NULL) {
        mutex->unlock(mutex->context);
    }
}

#endif
