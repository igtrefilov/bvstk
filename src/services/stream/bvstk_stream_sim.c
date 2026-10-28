#include "services/stream/bvstk_stream_sim.h"

#include <string.h>

static int service_index(uint8_t service)
{
    if (service < DCP2_STREAM_SERVICE_I2C ||
        service > DCP2_STREAM_SERVICE_UART) {
        return -1;
    }
    return (int)(service - DCP2_STREAM_SERVICE_I2C);
}

static uint32_t endpoint_for_service(uint8_t service)
{
    switch (service) {
    case DCP2_STREAM_SERVICE_I2C:
        return 0x13u;
    case DCP2_STREAM_SERVICE_MDIO:
        return 0x01u;
    case DCP2_STREAM_SERVICE_SPI:
    case DCP2_STREAM_SERVICE_UART:
    default:
        return 0u;
    }
}

static bool pattern_valid(dcp2_stream_sim_pattern_t pattern)
{
    return pattern == DCP2_STREAM_SIM_PATTERN_COUNTER ||
           pattern == DCP2_STREAM_SIM_PATTERN_RAMP ||
           pattern == DCP2_STREAM_SIM_PATTERN_TOGGLE;
}

void bvstk_stream_sim_init(bvstk_stream_sim_t *sim, const bvstk_guard_t *guard)
{
    memset(sim, 0, sizeof(*sim));
    if (guard) sim->guard = *guard;
    for (unsigned i = 0; i < 4U; ++i) {
        sim->states[i].period_ms = DCP2_STREAM_SIM_DEFAULT_PERIOD_MS;
        sim->states[i].pattern = DCP2_STREAM_SIM_PATTERN_COUNTER;
        sim->states[i].words = DCP2_STREAM_SIM_DEFAULT_WORDS;
    }
}

bool bvstk_stream_sim_configure(bvstk_stream_sim_t *sim, uint32_t current_ms, uint8_t service,
                               bool enabled,
                               uint32_t period_ms,
                               dcp2_stream_sim_pattern_t pattern,
                               uint16_t words)
{
    int index = service_index(service);

    if (!sim || index < 0) {
        return false;
    }
    if (enabled && (period_ms < DCP2_STREAM_SIM_MIN_PERIOD_MS ||
                    period_ms > DCP2_STREAM_SIM_MAX_PERIOD_MS)) {
        return false;
    }
    if (!pattern_valid(pattern) || words == 0U ||
        words > DCP2_STREAM_SIM_MAX_WORDS) {
        return false;
    }

    if (sim->guard.enter) sim->guard.enter(sim->guard.context);
    sim->states[index].period_ms = period_ms;
    sim->states[index].pattern = pattern;
    sim->states[index].words = words;
    sim->states[index].sample_count = 0U;
    sim->states[index].emitted_count = 0U;
    sim->states[index].lost_count = 0U;
    sim->states[index].enabled = enabled;
    sim->states[index].next_due_ms = enabled ? current_ms + period_ms : 0U;
    if (sim->guard.leave) sim->guard.leave(sim->guard.context);
    return true;
}

bool bvstk_stream_sim_get_status(bvstk_stream_sim_t *sim, uint8_t service,
                                dcp2_stream_sim_status_t *status)
{
    int index = service_index(service);

    if (!sim || index < 0 || status == NULL) {
        return false;
    }
    if (sim->guard.enter) sim->guard.enter(sim->guard.context);
    status->enabled = sim->states[index].enabled;
    status->period_ms = sim->states[index].period_ms;
    status->pattern = sim->states[index].pattern;
    status->words = sim->states[index].words;
    status->emitted_count = sim->states[index].emitted_count;
    status->lost_count = sim->states[index].lost_count;
    if (sim->guard.leave) sim->guard.leave(sim->guard.context);
    return true;
}

void bvstk_stream_sim_reset_lost(bvstk_stream_sim_t *sim, uint32_t current_ms, uint8_t service)
{
    int index = service_index(service);

    if (!sim || index < 0) {
        return;
    }
    if (sim->guard.enter) sim->guard.enter(sim->guard.context);
    sim->states[index].lost_count = 0U;
    if (sim->states[index].enabled) {
        sim->states[index].next_due_ms = current_ms + sim->states[index].period_ms;
    }
    if (sim->guard.leave) sim->guard.leave(sim->guard.context);
}

static uint32_t make_word(uint8_t service,
                          dcp2_stream_sim_pattern_t pattern,
                          uint32_t sample,
                          uint16_t index,
                          uint16_t words)
{
    uint32_t value;

    switch (pattern) {
    case DCP2_STREAM_SIM_PATTERN_RAMP:
        value = (sample - 1U) * (uint32_t)words + (uint32_t)index;
        break;
    case DCP2_STREAM_SIM_PATTERN_TOGGLE:
        value = ((sample + (uint32_t)index) & 1U) == 0U
                    ? UINT32_C(0xAAAAAAAA)
                    : UINT32_C(0x55555555);
        break;
    case DCP2_STREAM_SIM_PATTERN_COUNTER:
    default:
        value = (sample << 8) | (uint32_t)index;
        break;
    }

    if (index == 0U) {
        return sample;
    }
    if (index == 1U) {
        return endpoint_for_service(service);
    }
    if (index == 2U) {
        return (sample - 1U) & UINT32_C(0xFF);
    }
    if (index == 3U && pattern == DCP2_STREAM_SIM_PATTERN_COUNTER) {
        return UINT32_C(0xC0000000) | sample;
    }
    return value;
}

static void write_be32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

bool bvstk_stream_sim_next(bvstk_stream_sim_t *sim, uint32_t current_ms, uint8_t service, dcp2_stream_sim_event_t *event)
{
    int index = service_index(service);
    uint32_t late_ms;
    uint32_t lost_delta;
    uint32_t sample;
    uint16_t word;

    if (!sim || index < 0 || event == NULL) {
        return false;
    }

    if (sim->guard.enter) sim->guard.enter(sim->guard.context);
    if (!sim->states[index].enabled ||
        (int32_t)(current_ms - sim->states[index].next_due_ms) < 0) {
        if (sim->guard.leave) sim->guard.leave(sim->guard.context);
        return false;
    }

    late_ms = current_ms - sim->states[index].next_due_ms;
    lost_delta = late_ms / sim->states[index].period_ms;
    sample = ++sim->states[index].sample_count;
    sim->states[index].emitted_count++;
    sim->states[index].lost_count += lost_delta;
    sim->states[index].next_due_ms = current_ms + sim->states[index].period_ms;

    memset(event, 0, sizeof(*event));
    event->service = service;
    event->time_us = (uint64_t)current_ms * 1000ull;
    event->ev_flags = lost_delta != 0U ? 1U : 0U;
    event->lost_delta = lost_delta;
    event->data_len = (uint16_t)(sim->states[index].words * sizeof(uint32_t));
    for (word = 0U; word < sim->states[index].words; ++word) {
        write_be32(event->data + (size_t)word * sizeof(uint32_t),
                   make_word(service,
                             sim->states[index].pattern,
                             sample,
                             word,
                             sim->states[index].words));
    }
    if (sim->guard.leave) sim->guard.leave(sim->guard.context);
    return true;
}

const char *dcp2_stream_sim_service_name(uint8_t service)
{
    switch (service) {
    case DCP2_STREAM_SERVICE_I2C:
        return "I2C";
    case DCP2_STREAM_SERVICE_MDIO:
        return "MDIO";
    case DCP2_STREAM_SERVICE_SPI:
        return "SPI";
    case DCP2_STREAM_SERVICE_UART:
        return "UART";
    default:
        return "UNKNOWN";
    }
}

const char *dcp2_stream_sim_pattern_name(dcp2_stream_sim_pattern_t pattern)
{
    switch (pattern) {
    case DCP2_STREAM_SIM_PATTERN_COUNTER:
        return "counter";
    case DCP2_STREAM_SIM_PATTERN_RAMP:
        return "ramp";
    case DCP2_STREAM_SIM_PATTERN_TOGGLE:
        return "toggle";
    default:
        return "unknown";
    }
}
