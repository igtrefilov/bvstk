#include "shared/events/bvstk_journal.h"
#include <string.h>

int bvstk_event_is_telemetry(uint16_t type)
{
    return type >= BVSTK_EVENT_CAPTURE_WRITE && type <= BVSTK_EVENT_SLAVE_REPLY_PREPARED;
}
void bvstk_journal_publish(bvstk_journal_t *journal, const bvstk_event_t *event)
{
    bvstk_journal_ring_t *ring;
    if (!journal || !event || event->bus >= BVSTK_JOURNAL_BUSES) return;
    if (!bvstk_event_is_telemetry(event->type) &&
        (event->type < BVSTK_EVENT_REG_ATTEMPT || event->type > BVSTK_EVENT_FAULT)) return;
    ring = &journal->rings[event->bus][bvstk_event_is_telemetry(event->type) ?
        BVSTK_JOURNAL_TELEMETRY : BVSTK_JOURNAL_EVENTS];
    ring->entries[ring->head % BVSTK_JOURNAL_CAPACITY] = *event;
    ++ring->head;
}
uint64_t bvstk_journal_head(const bvstk_journal_t *journal, unsigned bus, unsigned channel)
{
    return journal && bus < BVSTK_JOURNAL_BUSES && channel < 2U ?
        journal->rings[bus][channel].head : 0;
}
void bvstk_journal_read(const bvstk_journal_t *journal, unsigned bus, unsigned channel,
    int address, uint64_t after, bvstk_journal_batch_t *out)
{
    const bvstk_journal_ring_t *ring;
    uint64_t first;
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!journal || bus >= BVSTK_JOURNAL_BUSES || channel >= 2U) return;
    ring = &journal->rings[bus][channel];
    out->head = ring->head;
    first = ring->head > BVSTK_JOURNAL_CAPACITY ? ring->head - BVSTK_JOURNAL_CAPACITY : 0;
    if (after == BVSTK_JOURNAL_HISTORY) after = first;
    else if (after > ring->head) { out->reset = 1; after = first; }
    if (after < first) {
        out->lost = first - after;
        after = first;
    }
    while (after < ring->head && out->count < BVSTK_JOURNAL_BATCH) {
        const bvstk_event_t *e = &ring->entries[after % BVSTK_JOURNAL_CAPACITY];
        ++after;
        if (address >= 0 && e->arg0 != (uint32_t)address) continue;
        out->records[out->count].sequence = after;
        out->records[out->count++].event = *e;
    }
    out->next = after;
}
