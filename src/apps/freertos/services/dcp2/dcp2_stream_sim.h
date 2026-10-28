#ifndef BVSTK_FREERTOS_DCP2_STREAM_SIM_H
#define BVSTK_FREERTOS_DCP2_STREAM_SIM_H
#include "services/stream/bvstk_stream_sim.h"
bvstk_stream_sim_t *dcp2_stream_sim_instance(void);
bool dcp2_stream_sim_init(void);
bool dcp2_stream_sim_configure(uint8_t, bool, uint32_t, dcp2_stream_sim_pattern_t, uint16_t);
bool dcp2_stream_sim_get_status(uint8_t, dcp2_stream_sim_status_t *);
void dcp2_stream_sim_reset_lost(uint8_t);
bool dcp2_stream_sim_next(uint8_t, dcp2_stream_sim_event_t *);
#endif
