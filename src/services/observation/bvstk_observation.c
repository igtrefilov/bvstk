#include "services/observation/bvstk_observation.h"
#include <string.h>

static void enter(bvstk_observation_t *o)
{
    if (o->guard.enter) o->guard.enter(o->guard.context);
}
static void leave(bvstk_observation_t *o)
{
    if (o->guard.leave) o->guard.leave(o->guard.context);
}
void bvstk_observation_init(bvstk_observation_t *o, const bvstk_guard_t *guard)
{
    memset(o, 0, sizeof(*o));
    if (guard) o->guard = *guard;
}
void bvstk_observation_publish(void *context, const bvstk_event_t *event)
{
    bvstk_observation_t *o = context;
    if (!o || !event ||
        ((event->flags & BVSTK_EVENT_FLAG_TELEMETRY_UNCONFIRMED) &&
         bvstk_event_is_telemetry(event->type))) return;
    enter(o);
    bvstk_journal_publish(&o->journal, event);
    leave(o);
}
uint64_t bvstk_observation_head(bvstk_observation_t *o, unsigned bus, unsigned channel)
{
    uint64_t head;
    if (!o) return 0;
    enter(o);
    head = bvstk_journal_head(&o->journal, bus, channel);
    leave(o);
    return head;
}
void bvstk_observation_read(bvstk_observation_t *o, unsigned bus, unsigned channel,
    int address, int device_slot, uint64_t after, bvstk_journal_batch_t *out)
{
    if (!out) return;
    if (!o) { memset(out, 0, sizeof(*out)); return; }
    enter(o);
    bvstk_journal_read_device(&o->journal, bus, channel, address, device_slot, after, out);
    leave(o);
}
