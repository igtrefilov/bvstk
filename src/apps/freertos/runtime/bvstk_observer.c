#include "apps/freertos/runtime/bvstk_observer.h"
#include "FreeRTOS.h"
#include "task.h"
#include "services/observation/bvstk_observation.h"

/* Keep the large zero-initialized journal in BSS. The port supplies the short
 * critical section around the shared operation. */
static bvstk_observation_t observer;

void bvstk_observer_publish(void *context, const bvstk_event_t *event)
{
    (void)context;
    taskENTER_CRITICAL();
    bvstk_observation_publish(&observer, event);
    taskEXIT_CRITICAL();
}
uint64_t bvstk_observer_head(unsigned bus, unsigned channel)
{
    uint64_t head;
    taskENTER_CRITICAL();
    head = bvstk_observation_head(&observer, bus, channel);
    taskEXIT_CRITICAL();
    return head;
}
void bvstk_observer_read(unsigned bus, unsigned channel, int address,
    uint64_t after, bvstk_journal_batch_t *out)
{
    taskENTER_CRITICAL();
    bvstk_observation_read(&observer, bus, channel, address, -1, after, out);
    taskEXIT_CRITICAL();
}
void bvstk_observer_read_device(unsigned bus, unsigned channel, int device_slot,
    uint64_t after, bvstk_journal_batch_t *out)
{
    taskENTER_CRITICAL();
    bvstk_observation_read(&observer, bus, channel, -1, device_slot, after, out);
    taskEXIT_CRITICAL();
}
