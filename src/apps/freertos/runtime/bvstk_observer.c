#include "apps/freertos/runtime/bvstk_observer.h"
#include "FreeRTOS.h"
#include "task.h"

static bvstk_journal_t journal;

void bvstk_observer_publish(void *context, const bvstk_event_t *event)
{
    (void)context;
    if (event != NULL &&
        (event->flags & BVSTK_EVENT_FLAG_TELEMETRY_UNCONFIRMED) != 0U &&
        bvstk_event_is_telemetry(event->type)) {
        return;
    }
    /* Called from tasks only. No IO, allocation or blocking in this section. */
    taskENTER_CRITICAL();
    bvstk_journal_publish(&journal, event);
    taskEXIT_CRITICAL();
}
uint64_t bvstk_observer_head(unsigned bus, unsigned channel)
{
    uint64_t head;
    taskENTER_CRITICAL();
    head = bvstk_journal_head(&journal, bus, channel);
    taskEXIT_CRITICAL();
    return head;
}
void bvstk_observer_read(unsigned bus, unsigned channel, int address,
    uint64_t after, bvstk_journal_batch_t *out)
{
    taskENTER_CRITICAL();
    bvstk_journal_read(&journal, bus, channel, address, after, out);
    taskEXIT_CRITICAL();
}
