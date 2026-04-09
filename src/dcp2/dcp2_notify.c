#include "dcp2_notify.h"

#include <string.h>

enum { DCP2_NOTIFY_QUEUE_LEN = 16 };

static QueueHandle_t s_notify_queue = NULL;

static uint64_t dcp2_notify_now_us(void)
{
    TickType_t ticks = xTaskGetTickCount();
    return (uint64_t)ticks * (uint64_t)portTICK_PERIOD_MS * 1000ull;
}

uint32_t dcp2_notify_event_class_mask(uint16_t ev_type)
{
    switch (ev_type) {
    case DCP2_NOTIFY_EV_REG_ATTEMPT:
        return DCP2_NOTIFY_CLASS_REG_ATTEMPT;
    case DCP2_NOTIFY_EV_REG_COMMIT:
        return DCP2_NOTIFY_CLASS_REG_COMMIT;
    case DCP2_NOTIFY_EV_REG_DENIED:
        return DCP2_NOTIFY_CLASS_REG_DENIED;
    case DCP2_NOTIFY_EV_STATE_CHANGED:
        return DCP2_NOTIFY_CLASS_STATE_CHANGED;
    case DCP2_NOTIFY_EV_FAULT:
        return DCP2_NOTIFY_CLASS_FAULT;
    default:
        return 0u;
    }
}

bool dcp2_notify_init(void)
{
    taskENTER_CRITICAL();
    if (s_notify_queue == NULL) {
        s_notify_queue = xQueueCreate(DCP2_NOTIFY_QUEUE_LEN, sizeof(dcp2_notify_event_t));
    }
    taskEXIT_CRITICAL();
    return s_notify_queue != NULL;
}

bool dcp2_notify_receive(dcp2_notify_event_t *out, TickType_t timeout_ticks)
{
    if (!out || s_notify_queue == NULL) return false;
    return xQueueReceive(s_notify_queue, out, timeout_ticks) == pdTRUE;
}

void dcp2_notify_publish(const dcp2_notify_event_t *event)
{
    dcp2_notify_event_t local;

    if (!event) return;
    if (s_notify_queue == NULL && !dcp2_notify_init()) return;

    local = *event;
    if (local.time_us == 0u) {
        local.time_us = dcp2_notify_now_us();
    }
    (void)xQueueSendToBack(s_notify_queue, &local, 0);
}

void dcp2_notify_publish_simple(uint16_t ev_type,
                                uint16_t status,
                                dcp2_notify_source_t source,
                                dcp2_notify_bus_t bus,
                                dcp2_notify_op_t op_kind,
                                uint32_t arg0,
                                uint32_t arg1,
                                uint32_t arg2)
{
    dcp2_notify_event_t event;

    memset(&event, 0, sizeof(event));
    event.ev_type = ev_type;
    event.status = status;
    event.source = (uint8_t)source;
    event.bus = (uint8_t)bus;
    event.op_kind = (uint8_t)op_kind;
    event.arg0 = arg0;
    event.arg1 = arg1;
    event.arg2 = arg2;
    dcp2_notify_publish(&event);
}
