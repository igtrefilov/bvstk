#ifndef BVSTK_JOURNAL_H
#define BVSTK_JOURNAL_H

#include <stddef.h>
#include "shared/events/bvstk_event.h"

#define BVSTK_JOURNAL_CAPACITY 256U
#define BVSTK_JOURNAL_BATCH 32U
#define BVSTK_JOURNAL_BUSES 2U
#define BVSTK_JOURNAL_HISTORY UINT64_MAX
enum { BVSTK_JOURNAL_EVENTS = 0, BVSTK_JOURNAL_TELEMETRY = 1 };

typedef struct {
    uint64_t sequence;
    bvstk_event_t event;
} bvstk_journal_record_t;
typedef struct {
    uint64_t head;
    bvstk_event_t entries[BVSTK_JOURNAL_CAPACITY];
} bvstk_journal_ring_t;
typedef struct {
    bvstk_journal_ring_t rings[BVSTK_JOURNAL_BUSES][2];
} bvstk_journal_t;
typedef struct {
    uint64_t next, head, lost;
    size_t count;
    int reset;
    bvstk_journal_record_t records[BVSTK_JOURNAL_BATCH];
} bvstk_journal_batch_t;

/* Zero-initialized RAM, no allocation. Caller serializes each operation.
 * Cursor is the last scanned sequence, not the last matching record.
 * lost counts overwritten records of this BUS/channel, before address filtering.
 * HISTORY requests retained history without counting earlier history as lost. */
void bvstk_journal_publish(bvstk_journal_t *, const bvstk_event_t *);
uint64_t bvstk_journal_head(const bvstk_journal_t *, unsigned bus, unsigned channel);
void bvstk_journal_read(const bvstk_journal_t *, unsigned bus, unsigned channel,
    int address, uint64_t after, bvstk_journal_batch_t *);
int bvstk_event_is_telemetry(uint16_t type);

#endif
