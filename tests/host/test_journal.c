#include "shared/events/bvstk_journal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bvstk_journal_t journal;
static bvstk_journal_batch_t a, b;
int main(void)
{
    bvstk_event_t event = {0};
    event.type = BVSTK_EVENT_MASTER_READ; event.bus = BVSTK_EVENT_BUS_MDIO;
    event.arg0 = 1; event.arg1 = 2; event.arg2 = 7; event.time_us = 123000;
    bvstk_journal_publish(&journal, &event);
    bvstk_journal_read(&journal, 1, 1, 1, 0, &a);
    bvstk_journal_read(&journal, 1, 1, 1, 0, &b);
    assert(a.count == 1 && !memcmp(&a, &b, sizeof(a)));
    assert(a.next == 1 && a.records[0].event.time_us == event.time_us);
    bvstk_journal_read(&journal, 1, 1, 2, 0, &a);
    assert(!a.count && a.next == 1);
    assert(!bvstk_journal_head(&journal, 1, 0));
    for (unsigned i = 0; i < BVSTK_JOURNAL_CAPACITY + 10; ++i)
        bvstk_journal_publish(&journal, &event);
    bvstk_journal_read(&journal, 1, 1, 1, 1, &a);
    assert(a.lost == 10 && a.count == BVSTK_JOURNAL_BATCH && a.records[0].sequence == 12);
    bvstk_journal_read(&journal, 1, 1, 1, 0, &a);
    assert(a.lost == 11);
    bvstk_journal_read(&journal, 1, 1, 1, BVSTK_JOURNAL_HISTORY, &a);
    assert(!a.lost && !a.reset && a.records[0].sequence == 12);
    bvstk_journal_read(&journal, 1, 1, 2, 1, &a);
    assert(a.lost == 10 && !a.count && a.next == a.head);
    bvstk_journal_read(&journal, 1, 1, -1, 10000, &a);
    assert(a.reset && !a.lost && a.count == BVSTK_JOURNAL_BATCH);
    event.type = BVSTK_EVENT_REG_DENIED;
    bvstk_journal_publish(&journal, &event);
    assert(bvstk_journal_head(&journal, 1, 0) == 1);
    assert(!bvstk_journal_head(&journal, 0, 0));
    bvstk_journal_read(&journal, 1, 0, -1, 0, &a);
    assert(a.count == 1 && a.records[0].event.type == BVSTK_EVENT_REG_DENIED);
    bvstk_journal_read(NULL, 0, 0, -1, 0, &a);
    assert(!a.count && !a.next);
    journal.rings[0][0].head = UINT64_C(0xFFFFFFFF);
    event.bus = 0;
    bvstk_journal_publish(&journal, &event);
    bvstk_journal_read(&journal, 0, 0, -1, UINT64_C(0xFFFFFFFF), &a);
    assert(a.next == UINT64_C(0x100000000) && a.count == 1);
    puts("Observation journal tests passed");
    return 0;
}
