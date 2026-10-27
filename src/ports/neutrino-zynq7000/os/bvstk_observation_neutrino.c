#include "ports/neutrino-zynq7000/os/bvstk_observation_neutrino.h"
#include <string.h>

static void enter(void *context) { (void)pthread_mutex_lock(context); }
static void leave(void *context) { (void)pthread_mutex_unlock(context); }

int bvstk_observation_neutrino_init(bvstk_observation_neutrino_t *o)
{
    pthread_mutexattr_t attr;
    if (!o) return 0;
    memset(o, 0, sizeof(*o));
    if (pthread_mutexattr_init(&attr)) return 0;
    if (pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT) ||
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) ||
        pthread_mutex_init(&o->cache_mutex, &attr)) {
        pthread_mutexattr_destroy(&attr);
        return 0;
    }
    /* The I2C sink nests registry publication inside the mutation guard. */
    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL) ||
        pthread_mutex_init(&o->journal_mutex, &attr)) {
        pthread_mutexattr_destroy(&attr);
        pthread_mutex_destroy(&o->cache_mutex);
        return 0;
    }
    pthread_mutexattr_destroy(&attr);
    bvstk_guard_t cache = {&o->cache_mutex, enter, leave};
    bvstk_guard_t events = {&o->journal_mutex, enter, leave};
    bvstk_cache_init(&o->cache, &cache);
    bvstk_observation_init(&o->events, &events);
    o->initialized = 1;
    return 1;
}
void bvstk_observation_neutrino_destroy(bvstk_observation_neutrino_t *o)
{
    if (!o || !o->initialized) return;
    pthread_mutex_destroy(&o->journal_mutex);
    pthread_mutex_destroy(&o->cache_mutex);
    o->initialized = 0;
}
