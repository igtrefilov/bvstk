#ifndef BVSTK_OBSERVATION_H
#define BVSTK_OBSERVATION_H

#include "shared/events/bvstk_journal.h"
#include "shared/interfaces/bvstk_guard.h"

typedef struct {
    bvstk_journal_t journal;
    bvstk_guard_t guard;
} bvstk_observation_t;

/* Initialize before exposing the instance to producers or readers. */
void bvstk_observation_init(bvstk_observation_t *, const bvstk_guard_t *);
/* Event-sink callback shared by all targets. Unconfirmed telemetry and quiet
 * presence/acquisition traffic do not enter the user-facing journal. */
void bvstk_observation_publish(void *, const bvstk_event_t *);
uint64_t bvstk_observation_head(bvstk_observation_t *, unsigned bus, unsigned channel);
/* Copy a bounded batch under the guard, then release it before formatting or
 * transport I/O. -1 disables the address or device-slot filter. */
void bvstk_observation_read(bvstk_observation_t *, unsigned bus, unsigned channel,
    int address, int device_slot, uint64_t after, bvstk_journal_batch_t *);

#endif
