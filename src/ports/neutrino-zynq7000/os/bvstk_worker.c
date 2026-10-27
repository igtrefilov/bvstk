#include "ports/neutrino-zynq7000/os/bvstk_worker.h"
#include "shared/interfaces/bvstk_clock.h"

static void *run(void *context)
{
    bvstk_neutrino_worker_t *worker = context;
    for (;;) {
        pthread_mutex_lock(&worker->mutex);
        int stop = worker->stopping;
        pthread_mutex_unlock(&worker->mutex);
        if (stop) return NULL;
        worker->step(worker->context);
        bvstk_platform_sleep_ms(10U);
    }
}

bvstk_status_t bvstk_neutrino_worker_start(bvstk_neutrino_worker_t *worker,
    void (*step)(void *), void *context)
{
    if (!worker || !step) return BVSTK_ERR_MALFORMED;
    if (worker->started) return BVSTK_ERR_BUSY;
    if (pthread_mutex_init(&worker->mutex, NULL)) return BVSTK_ERR_INTERNAL;
    worker->stopping = 0;
    worker->step = step;
    worker->context = context;
    if (pthread_create(&worker->thread, NULL, run, worker)) {
        pthread_mutex_destroy(&worker->mutex);
        return BVSTK_ERR_INTERNAL;
    }
    worker->started = 1;
    return BVSTK_OK;
}

void bvstk_neutrino_worker_stop(bvstk_neutrino_worker_t *worker)
{
    if (!worker || !worker->started) return;
    pthread_mutex_lock(&worker->mutex);
    worker->stopping = 1;
    pthread_mutex_unlock(&worker->mutex);
    pthread_join(worker->thread, NULL);
    pthread_mutex_destroy(&worker->mutex);
    worker->started = 0;
}
