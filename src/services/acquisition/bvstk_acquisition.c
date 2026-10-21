#include "services/acquisition/bvstk_acquisition.h"
#include <string.h>

static void invalidate_group(const bvstk_acquisition_group_t *g,
                              const bvstk_acquisition_io_t *io)
{
    if (!io->invalidate) return;
    for (unsigned r = 0; r < g->range_count; ++r)
        io->invalidate(io->context, g->ranges[r].first, g->ranges[r].count);
}

int bvstk_acquisition_step(const bvstk_acquisition_plan_t *p,
    bvstk_acquisition_state_t *s, const bvstk_acquisition_io_t *io, uint64_t now)
{
    if (!p || !s || !io || !io->read || !io->ready || !io->cached ||
        !p->enabled || !p->group_count || p->group_count > BVSTK_ACQUISITION_GROUPS_MAX)
        return 0;
    int ready = io->ready(io->context);
    /* A temporary inability to acquire a service mutex is not a disconnect. */
    if (ready < 0) return 0;
    if (!ready) {
        if (s->connected) {
            for (unsigned i = 0; i < p->group_count; ++i) invalidate_group(&p->groups[i], io);
            memset(s, 0, sizeof(*s));
        }
        return 0;
    }
    if (!s->connected) { memset(s, 0, sizeof(*s)); s->connected = 1; }
    if (now < s->due) return 0;
    for (unsigned n = 0; n < p->group_count; ++n) {
        unsigned i = s->next_group;
        s->next_group = (uint8_t)((i + 1U) % p->group_count);
        const bvstk_acquisition_group_t *g = &p->groups[i];
        bvstk_acquisition_group_state_t *state = &s->groups[i];
        int allowed = 1;
        for (unsigned c = 0; c < g->condition_count; ++c) {
            uint16_t value;
            if (!io->cached(io->context, g->conditions[c].resource, &value) ||
                (value & g->conditions[c].mask) != g->conditions[c].value) {
                allowed = 0; break;
            }
        }
        if (!allowed) {
            if (state->range || state->offset || state->complete) invalidate_group(g, io);
            memset(state, 0, sizeof(*state));
            continue;
        }
        if ((state->complete && !g->interval_ms) || now < state->due) continue;
        if (!g->range_count || state->range >= g->range_count || !g->chunk_size) continue;
        const bvstk_acquisition_range_t *r = &g->ranges[state->range];
        unsigned count = r->count - state->offset;
        if (count > g->chunk_size) count = g->chunk_size;
        if (!count || count > BVSTK_ACQUISITION_CHUNK_MAX) continue;
        bvstk_status_t status = io->read(io->context, r->first + state->offset, count);
        s->due = now + (p->read_delay_ms ? p->read_delay_ms : 1U);
        if (status != BVSTK_OK) {
            /* Retry this chunk, allowing unrelated groups/devices to progress. */
            state->due = now + p->retry_delay_ms;
        } else {
            state->offset += (uint16_t)count;
            if (state->offset == r->count) { state->offset = 0; ++state->range; }
            if (state->range == g->range_count) {
                state->range = 0; state->complete = 1;
                state->due = now + g->interval_ms;
            }
        }
        return 1;
    }
    return 0;
}
