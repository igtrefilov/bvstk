#ifndef BVSTK_FREERTOS_SYNC_H
#define BVSTK_FREERTOS_SYNC_H

#include "FreeRTOS.h"
#include "semphr.h"

#include "shared/interfaces/bvstk_sync.h"

typedef struct {
    SemaphoreHandle_t handle;
    bvstk_mutex_t public_mutex;
} bvstk_freertos_mutex_t;

bvstk_status_t bvstk_freertos_mutex_init(bvstk_freertos_mutex_t *mutex);
void bvstk_freertos_mutex_destroy(bvstk_freertos_mutex_t *mutex);

#endif
