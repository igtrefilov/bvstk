#ifndef BVSTK_FREERTOS_DCP2_STREAM_SIM_H
#define BVSTK_FREERTOS_DCP2_STREAM_SIM_H

#include <stdbool.h>
#include <stdint.h>

enum {
    DCP2_STREAM_SERVICE_I2C = 0x02,
    DCP2_STREAM_SERVICE_SMI = 0x03,
    DCP2_STREAM_SERVICE_SPI = 0x04,
    DCP2_STREAM_SERVICE_UART = 0x05,
    DCP2_STREAM_SIM_DEFAULT_PERIOD_MS = 100U,
    DCP2_STREAM_SIM_MIN_PERIOD_MS = 10U,
    DCP2_STREAM_SIM_MAX_PERIOD_MS = 60000U,
    DCP2_STREAM_SIM_DEFAULT_WORDS = 4U,
    DCP2_STREAM_SIM_MAX_WORDS = 64U,
};

typedef enum {
    DCP2_STREAM_SIM_PATTERN_COUNTER = 0,
    DCP2_STREAM_SIM_PATTERN_RAMP = 1,
    DCP2_STREAM_SIM_PATTERN_TOGGLE = 2,
} dcp2_stream_sim_pattern_t;

typedef struct {
    bool enabled;
    uint32_t period_ms;
    dcp2_stream_sim_pattern_t pattern;
    uint16_t words;
    uint32_t emitted_count;
    uint32_t lost_count;
} dcp2_stream_sim_status_t;

typedef struct {
    uint8_t service;
    uint64_t time_us;
    uint8_t ev_flags;
    uint32_t lost_delta;
    uint16_t data_len;
    uint8_t data[DCP2_STREAM_SIM_MAX_WORDS * sizeof(uint32_t)];
} dcp2_stream_sim_event_t;

bool dcp2_stream_sim_init(void);
bool dcp2_stream_sim_configure(uint8_t service,
                               bool enabled,
                               uint32_t period_ms,
                               dcp2_stream_sim_pattern_t pattern,
                               uint16_t words);
bool dcp2_stream_sim_get_status(uint8_t service,
                                dcp2_stream_sim_status_t *status);
void dcp2_stream_sim_reset_lost(uint8_t service);
bool dcp2_stream_sim_next(uint8_t service, dcp2_stream_sim_event_t *event);

const char *dcp2_stream_sim_service_name(uint8_t service);
const char *dcp2_stream_sim_pattern_name(dcp2_stream_sim_pattern_t pattern);

#endif /* BVSTK_FREERTOS_DCP2_STREAM_SIM_H */
