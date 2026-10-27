#ifndef BVSTK_NEUTRINO_OBSERVE_H
#define BVSTK_NEUTRINO_OBSERVE_H

#include "apps/neutrino/bvstkd/bvstk_dcp2_runtime.h"
#include "services/observation/bvstk_observation.h"

#define BVSTK_OBSERVE_VERSION 1U
typedef struct {
    uint32_t version, bus, channel;
    int32_t address;
    uint64_t after;
    char device[32];
    /* Response: a passive bounded copy, independent of subsequent events. */
    uint32_t status;
    uint64_t uptime_ms;
    bvstk_journal_batch_t batch;
} bvstk_observe_message_t;

void bvstk_neutrino_observe_query(bvstk_observation_t *,
    bvstk_neutrino_dcp2_runtime_t *, bvstk_observe_message_t *);

#endif
