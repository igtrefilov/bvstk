#ifndef BVSTK_NEUTRINO_SYNC_H
#define BVSTK_NEUTRINO_SYNC_H

#include <pthread.h>

#include "shared/interfaces/bvstk_sync.h"

typedef struct {
    pthread_mutex_t native;
    bvstk_mutex_t public_mutex;
} bvstk_neutrino_mutex_t;

bvstk_status_t bvstk_neutrino_mutex_init(bvstk_neutrino_mutex_t *mutex);
void bvstk_neutrino_mutex_destroy(bvstk_neutrino_mutex_t *mutex);

#endif
