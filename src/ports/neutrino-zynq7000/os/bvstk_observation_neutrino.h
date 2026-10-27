#ifndef BVSTK_OBSERVATION_NEUTRINO_H
#define BVSTK_OBSERVATION_NEUTRINO_H

#include <pthread.h>
#include "services/cache/bvstk_cache.h"
#include "services/observation/bvstk_observation.h"

typedef struct {
    bvstk_cache_registry_t cache;
    bvstk_observation_t events;
    pthread_mutex_t cache_mutex, journal_mutex;
    int initialized;
} bvstk_observation_neutrino_t;

int bvstk_observation_neutrino_init(bvstk_observation_neutrino_t *);
/* Owner must stop all providers and readers before destroying the guards. */
void bvstk_observation_neutrino_destroy(bvstk_observation_neutrino_t *);

#endif
