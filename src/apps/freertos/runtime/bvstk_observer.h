#ifndef BVSTK_FREERTOS_OBSERVER_H
#define BVSTK_FREERTOS_OBSERVER_H
#include "shared/events/bvstk_journal.h"
void bvstk_observer_publish(void *context, const bvstk_event_t *event);
uint64_t bvstk_observer_head(unsigned bus, unsigned channel);
void bvstk_observer_read(unsigned bus, unsigned channel, int address,
    uint64_t after, bvstk_journal_batch_t *out);
#endif
