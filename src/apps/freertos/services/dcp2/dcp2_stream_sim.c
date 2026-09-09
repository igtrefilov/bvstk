#include "apps/freertos/services/dcp2/dcp2_stream_sim.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

enum {
    DCP2_STREAM_SIM_BUS_COUNT = 4,
    DCP2_STREAM_SIM_OVERFLOW_FLAG = 1u << 0,
};

typedef struct {
    bool enabled;
    uint32_t period_ms;
    dcp2_stream_sim_pattern_t pattern;
    uint16_t words;
    uint32_t sample_count;
    uint32_t emitted_count;
    uint32_t lost_count;
    uint32_t next_due_ms;
} dcp2_stream_sim_state_t;

static dcp2_stream_sim_state_t s_states[DCP2_STREAM_SIM_BUS_COUNT];
static bool s_initialized;

static int service_index(uint8_t service)
{
    if (service < DCP2_STREAM_SERVICE_I2C ||
        service > DCP2_STREAM_SERVICE_UART) {
        return -1;
    }
    return (int)(service - DCP2_STREAM_SERVICE_I2C);
}

static uint32_t now_ms(void)
{
    return (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;
}

static uint32_t endpoint_for_service(uint8_t service)
{
    switch (service) {
    case DCP2_STREAM_SERVICE_I2C:
        return 0x13u;
    case DCP2_STREAM_SERVICE_SMI:
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

static void ensure_initialized_locked(void)
{
    size_t i;

    if (s_initialized) {
        return;
    }
    for (i = 0U; i < DCP2_STREAM_SIM_BUS_COUNT; ++i) {
        s_states[i].period_ms = DCP2_STREAM_SIM_DEFAULT_PERIOD_MS;
        s_states[i].pattern = DCP2_STREAM_SIM_PATTERN_COUNTER;
        s_states[i].words = DCP2_STREAM_SIM_DEFAULT_WORDS;
    }
    s_initialized = true;
}

bool dcp2_stream_sim_init(void)
{
    taskENTER_CRITICAL();
    ensure_initialized_locked();
    taskEXIT_CRITICAL();
    return true;
}

bool dcp2_stream_sim_configure(uint8_t service,
                               bool enabled,
                               uint32_t period_ms,
                               dcp2_stream_sim_pattern_t pattern,
                               uint16_t words)
{
    int index = service_index(service);
    uint32_t current_ms;

    if (index < 0) {
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

    current_ms = now_ms();
    taskENTER_CRITICAL();
    ensure_initialized_locked();
    s_states[index].period_ms = period_ms;
    s_states[index].pattern = pattern;
    s_states[index].words = words;
    s_states[index].sample_count = 0U;
    s_states[index].emitted_count = 0U;
    s_states[index].lost_count = 0U;
    s_states[index].enabled = enabled;
    s_states[index].next_due_ms = enabled ? current_ms + period_ms : 0U;
    taskEXIT_CRITICAL();
    return true;
}

bool dcp2_stream_sim_get_status(uint8_t service,
                                dcp2_stream_sim_status_t *status)
{
    int index = service_index(service);

    if (index < 0 || status == NULL) {
        return false;
    }
    taskENTER_CRITICAL();
    ensure_initialized_locked();
    status->enabled = s_states[index].enabled;
    status->period_ms = s_states[index].period_ms;
    status->pattern = s_states[index].pattern;
    status->words = s_states[index].words;
    status->emitted_count = s_states[index].emitted_count;
    status->lost_count = s_states[index].lost_count;
    taskEXIT_CRITICAL();
    return true;
}

void dcp2_stream_sim_reset_lost(uint8_t service)
{
    int index = service_index(service);
    uint32_t current_ms;

    if (index < 0) {
        return;
    }
    current_ms = now_ms();
    taskENTER_CRITICAL();
    ensure_initialized_locked();
    s_states[index].lost_count = 0U;
    if (s_states[index].enabled) {
        s_states[index].next_due_ms = current_ms + s_states[index].period_ms;
    }
    taskEXIT_CRITICAL();
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

bool dcp2_stream_sim_next(uint8_t service, dcp2_stream_sim_event_t *event)
{
    int index = service_index(service);
    uint32_t current_ms;
    uint32_t late_ms;
    uint32_t lost_delta;
    uint32_t sample;
    uint16_t word;

    if (index < 0 || event == NULL) {
        return false;
    }

    current_ms = now_ms();
    taskENTER_CRITICAL();
    ensure_initialized_locked();
    if (!s_states[index].enabled ||
        (int32_t)(current_ms - s_states[index].next_due_ms) < 0) {
        taskEXIT_CRITICAL();
        return false;
    }

    late_ms = current_ms - s_states[index].next_due_ms;
    lost_delta = late_ms / s_states[index].period_ms;
    sample = ++s_states[index].sample_count;
    s_states[index].emitted_count++;
    s_states[index].lost_count += lost_delta;
    s_states[index].next_due_ms = current_ms + s_states[index].period_ms;

    memset(event, 0, sizeof(*event));
    event->service = service;
    event->time_us = (uint64_t)current_ms * 1000ull;
    event->ev_flags = lost_delta != 0U ? DCP2_STREAM_SIM_OVERFLOW_FLAG : 0U;
    event->lost_delta = lost_delta;
    event->data_len = (uint16_t)(s_states[index].words * sizeof(uint32_t));
    for (word = 0U; word < s_states[index].words; ++word) {
        write_be32(event->data + (size_t)word * sizeof(uint32_t),
                   make_word(service,
                             s_states[index].pattern,
                             sample,
                             word,
                             s_states[index].words));
    }
    taskEXIT_CRITICAL();
    return true;
}

const char *dcp2_stream_sim_service_name(uint8_t service)
{
    switch (service) {
    case DCP2_STREAM_SERVICE_I2C:
        return "I2C";
    case DCP2_STREAM_SERVICE_SMI:
        return "SMI";
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
