#include "ports/freertos-xilinx/os/bvstk_sync_freertos.h"

static bvstk_status_t lock_mutex(void *context, uint32_t timeout_ms)
{
    bvstk_freertos_mutex_t *mutex = (bvstk_freertos_mutex_t *)context;
    TickType_t ticks;

    if (mutex == NULL || mutex->handle == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    ticks = pdMS_TO_TICKS(timeout_ms);
    if (timeout_ms != 0U && ticks == 0U) {
        ticks = 1U;
    }
    return xSemaphoreTake(mutex->handle, ticks) == pdTRUE
               ? BVSTK_OK
               : BVSTK_ERR_TIMEOUT;
}

static void unlock_mutex(void *context)
{
    bvstk_freertos_mutex_t *mutex = (bvstk_freertos_mutex_t *)context;
    if (mutex != NULL && mutex->handle != NULL) {
        (void)xSemaphoreGive(mutex->handle);
    }
}

bvstk_status_t bvstk_freertos_mutex_init(bvstk_freertos_mutex_t *mutex)
{
    if (mutex == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    mutex->handle = xSemaphoreCreateMutex();
    if (mutex->handle == NULL) {
        return BVSTK_ERR_INTERNAL;
    }
    mutex->public_mutex.context = mutex;
    mutex->public_mutex.lock = lock_mutex;
    mutex->public_mutex.unlock = unlock_mutex;
    return BVSTK_OK;
}

void bvstk_freertos_mutex_destroy(bvstk_freertos_mutex_t *mutex)
{
    if (mutex != NULL && mutex->handle != NULL) {
        vSemaphoreDelete(mutex->handle);
        mutex->handle = NULL;
        mutex->public_mutex.context = NULL;
        mutex->public_mutex.lock = NULL;
        mutex->public_mutex.unlock = NULL;
    }
}
