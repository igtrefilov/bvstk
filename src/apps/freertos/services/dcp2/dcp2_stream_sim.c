#include "apps/freertos/services/dcp2/dcp2_stream_sim.h"
#include "shared/interfaces/bvstk_clock.h"
#include "FreeRTOS.h"
#include "task.h"
static bvstk_stream_sim_t streams;
static bool initialized;
static void enter(void *context) { (void)context; taskENTER_CRITICAL(); }
static void leave(void *context) { (void)context; taskEXIT_CRITICAL(); }
bvstk_stream_sim_t *dcp2_stream_sim_instance(void)
{
    taskENTER_CRITICAL();
    if (!initialized) {
        const bvstk_guard_t guard = {NULL, enter, leave};
        bvstk_stream_sim_init(&streams, &guard);
        initialized = true;
    }
    taskEXIT_CRITICAL();
    return &streams;
}
bool dcp2_stream_sim_init(void) { return dcp2_stream_sim_instance() != NULL; }
bool dcp2_stream_sim_configure(uint8_t service, bool enabled, uint32_t period,
    dcp2_stream_sim_pattern_t pattern, uint16_t words)
{
    return bvstk_stream_sim_configure(dcp2_stream_sim_instance(),
        (uint32_t)bvstk_platform_now_ms(), service, enabled, period, pattern, words);
}
bool dcp2_stream_sim_get_status(uint8_t service, dcp2_stream_sim_status_t *status)
{ return bvstk_stream_sim_get_status(dcp2_stream_sim_instance(), service, status); }
void dcp2_stream_sim_reset_lost(uint8_t service)
{ bvstk_stream_sim_reset_lost(dcp2_stream_sim_instance(), (uint32_t)bvstk_platform_now_ms(), service); }
bool dcp2_stream_sim_next(uint8_t service, dcp2_stream_sim_event_t *event)
{ return bvstk_stream_sim_next(dcp2_stream_sim_instance(), (uint32_t)bvstk_platform_now_ms(), service, event); }
