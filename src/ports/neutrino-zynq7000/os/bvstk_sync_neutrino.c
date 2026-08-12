#include "ports/neutrino-zynq7000/os/bvstk_sync_neutrino.h"

#include <errno.h>

static bvstk_status_t lock_mutex(void *context, uint32_t timeout_ms)
{
    bvstk_neutrino_mutex_t *mutex = (bvstk_neutrino_mutex_t *)context;
    int result;

    (void)timeout_ms;
    if (mutex == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    result = pthread_mutex_lock(&mutex->native);
    if (result == 0) {
        return BVSTK_OK;
    }
    return result == ETIMEDOUT ? BVSTK_ERR_TIMEOUT : BVSTK_ERR_BUSY;
}

static void unlock_mutex(void *context)
{
    bvstk_neutrino_mutex_t *mutex = (bvstk_neutrino_mutex_t *)context;
    if (mutex != NULL) {
        (void)pthread_mutex_unlock(&mutex->native);
    }
}

bvstk_status_t bvstk_neutrino_mutex_init(bvstk_neutrino_mutex_t *mutex)
{
    if (mutex == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    if (pthread_mutex_init(&mutex->native, NULL) != 0) {
        return BVSTK_ERR_INTERNAL;
    }
    mutex->public_mutex.context = mutex;
    mutex->public_mutex.lock = lock_mutex;
    mutex->public_mutex.unlock = unlock_mutex;
    return BVSTK_OK;
}

void bvstk_neutrino_mutex_destroy(bvstk_neutrino_mutex_t *mutex)
{
    if (mutex != NULL) {
        (void)pthread_mutex_destroy(&mutex->native);
        mutex->public_mutex.context = NULL;
        mutex->public_mutex.lock = NULL;
        mutex->public_mutex.unlock = NULL;
    }
}
