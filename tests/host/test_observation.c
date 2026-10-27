#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "ports/neutrino-zynq7000/os/bvstk_observation_neutrino.h"

static bvstk_observation_neutrino_t observation;
static atomic_int done;
static uint16_t handle;
static unsigned described;

static void describe(void *context, unsigned r, bvstk_cache_entry_t *e)
{
    (void)context;
    ++described;
    snprintf(e->id, sizeof(e->id), "reg_%02x", r);
    e->width_bits = 32;
    e->valid = 1;
    e->source = BVSTK_CACHE_SOURCE_SETTINGS;
    e->value = r ? UINT32_MAX : 0;
}
static void *producer(void *argument)
{
    (void)argument;
    bvstk_cache_registry_t *r = &observation.cache;
    for (unsigned i = 1; i <= 4000U; ++i) {
        /* The real I2C/MDIO providers use this nested memory-only pattern. */
        r->guard.enter(r->guard.context);
        assert(bvstk_cache_put(r, handle, 0, i, 1, BVSTK_CACHE_SOURCE_DEVICE, i, i) == BVSTK_OK);
        assert(bvstk_cache_put(r, handle, 1, UINT32_MAX - i, 1,
            BVSTK_CACHE_SOURCE_DEVICE, i, i) == BVSTK_OK);
        r->guard.leave(r->guard.context);
        bvstk_event_t e = {.bus = BVSTK_EVENT_BUS_I2C, .type = BVSTK_EVENT_REG_COMMIT,
            .time_us = i * 1000U, .arg0 = 0x50, .arg1 = i, .arg2 = ~i};
        bvstk_observation_publish(&observation.events, &e);
    }
    atomic_store(&done, 1);
    return NULL;
}
static void *reader(void *argument)
{
    (void)argument;
    static bvstk_cache_snapshot_t snapshot;
    uint16_t h;
    uint64_t cursor, after = 0;
    do {
        assert(bvstk_cache_snapshot(&observation.cache, "i2c", "device", &snapshot,
            &h, &cursor, 123U) == BVSTK_OK);
        assert(h == handle && snapshot.count == 2 && snapshot.captured_ms == 123);
        assert(snapshot.entries[0].value + snapshot.entries[1].value == UINT32_MAX);
        assert(snapshot.entries[0].updated_ms == snapshot.entries[1].updated_ms);
        bvstk_journal_batch_t batch;
        bvstk_observation_read(&observation.events, 0, BVSTK_JOURNAL_EVENTS, -1, -1, after, &batch);
        for (size_t n = 0; n < batch.count; ++n) {
            const bvstk_event_t *e = &batch.records[n].event;
            assert(e->arg2 == ~e->arg1 && e->time_us == e->arg1 * 1000U);
            assert(batch.records[n].sequence > after);
        }
        after = batch.next;
    } while (!atomic_load(&done));
    return NULL;
}
int main(void)
{
    assert(bvstk_observation_neutrino_init(&observation));
    assert(bvstk_cache_define(&observation.cache, &handle, "i2c", "device", 2,
        BVSTK_CACHE_PRESENCE_UNKNOWN, describe, NULL) == BVSTK_OK);
    static bvstk_cache_snapshot_t snapshot;
    uint16_t h;
    uint64_t cursor;
    assert(bvstk_cache_snapshot(&observation.cache, "i2c", "device", &snapshot,
        &h, &cursor, 1) == BVSTK_OK);
    assert(snapshot.presence == BVSTK_CACHE_PRESENCE_UNKNOWN);
    assert(snapshot.entries[0].source == BVSTK_CACHE_SOURCE_SETTINGS);
    uint32_t generation = snapshot.generation;
    pthread_t writer, consumer;
    assert(!pthread_create(&consumer, NULL, reader, NULL));
    assert(!pthread_create(&writer, NULL, producer, NULL));
    assert(!pthread_join(writer, NULL) && !pthread_join(consumer, NULL));
    assert(described == 2); /* Snapshot reads never revisit the provider. */
    bvstk_cache_change_t change;
    assert(bvstk_cache_next(&observation.cache, handle, generation, &cursor, &change) == -BVSTK_CACHE_LOST);
    assert(bvstk_cache_snapshot(&observation.cache, "i2c", "device", &snapshot,
        &h, &cursor, 2) == BVSTK_OK);
    assert(bvstk_cache_next(&observation.cache, handle, generation, &cursor, &change) == 0);
    assert(bvstk_cache_remove(&observation.cache, handle) == BVSTK_OK);
    assert(bvstk_cache_next(&observation.cache, handle, generation, &cursor, &change) == -BVSTK_CACHE_RECONFIGURED);
    uint16_t replacement = 0;
    assert(bvstk_cache_define(&observation.cache, &replacement, "i2c", "device", 2,
        BVSTK_CACHE_PRESENCE_UNKNOWN, describe, NULL) == BVSTK_OK);
    assert(replacement == handle);
    assert(bvstk_cache_next(&observation.cache, handle, generation, &cursor, &change) == -BVSTK_CACHE_RECONFIGURED);

    bvstk_journal_batch_t batch;
    bvstk_observation_read(&observation.events, 0, BVSTK_JOURNAL_EVENTS, -1, -1, 0, &batch);
    assert(batch.head == 4000 && batch.lost == 4000 - BVSTK_JOURNAL_CAPACITY);
    bvstk_observation_read(&observation.events, 0, BVSTK_JOURNAL_EVENTS, -1, -1,
        BVSTK_JOURNAL_HISTORY, &batch);
    assert(!batch.lost && batch.count == BVSTK_JOURNAL_BATCH);
    bvstk_observation_read(&observation.events, 0, BVSTK_JOURNAL_EVENTS, 0x51, -1, 0, &batch);
    assert(!batch.count && batch.next == batch.head && batch.lost);
    bvstk_observation_read(&observation.events, 0, BVSTK_JOURNAL_EVENTS, -1, -1, 5000, &batch);
    assert(batch.reset && !batch.lost);

    bvstk_event_t e = {.bus = BVSTK_EVENT_BUS_I2C, .type = BVSTK_EVENT_MASTER_READ,
        .flags = BVSTK_EVENT_FLAG_TELEMETRY_UNCONFIRMED, .arg0 = 0x50};
    bvstk_observation_publish(&observation.events, &e);
    assert(!bvstk_observation_head(&observation.events, 0, BVSTK_JOURNAL_TELEMETRY));
    e.flags = BVSTK_EVENT_FLAG_DEVICE_SLOT | (2U << BVSTK_EVENT_DEVICE_SHIFT);
    bvstk_observation_publish(&observation.events, &e);
    e.flags = BVSTK_EVENT_FLAG_DEVICE_SLOT | (3U << BVSTK_EVENT_DEVICE_SHIFT);
    bvstk_observation_publish(&observation.events, &e);
    bvstk_observation_read(&observation.events, 0, BVSTK_JOURNAL_TELEMETRY, 0x50, 2, 0, &batch);
    assert(batch.count == 1 && batch.records[0].sequence == 1 && batch.next == 2);
    e.bus = BVSTK_EVENT_BUS_MDIO;
    e.type = BVSTK_EVENT_REG_DENIED;
    e.flags = BVSTK_EVENT_FLAG_TELEMETRY_UNCONFIRMED;
    bvstk_observation_publish(&observation.events, &e);
    assert(bvstk_observation_head(&observation.events, 1, BVSTK_JOURNAL_EVENTS) == 1);
    assert(!bvstk_observation_head(&observation.events, 1, BVSTK_JOURNAL_TELEMETRY));
    bvstk_observation_neutrino_destroy(&observation);
    puts("Observation: nested POSIX guards, concurrent coherent snapshots/journals, provenance, passive reads, loss/reset, device filters and provider removal passed");
    return 0;
}
