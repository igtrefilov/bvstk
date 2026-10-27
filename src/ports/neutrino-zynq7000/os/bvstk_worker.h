#ifndef BVSTK_NEUTRINO_WORKER_H
#define BVSTK_NEUTRINO_WORKER_H
#include <pthread.h>
#include "shared/base/bvstk_status.h"

/* A bounded step runs outside the stop mutex. Owner stops/joins before
 * destroying service state, and must not hold a service mutex while joining. */
typedef struct {
    pthread_t thread;
    pthread_mutex_t mutex;
    void (*step)(void *);
    void *context;
    int stopping;
    int started;
} bvstk_neutrino_worker_t;
bvstk_status_t bvstk_neutrino_worker_start(bvstk_neutrino_worker_t *, void (*)(void *), void *);
void bvstk_neutrino_worker_stop(bvstk_neutrino_worker_t *);
#endif
