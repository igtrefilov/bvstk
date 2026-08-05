#ifndef BVSTK_FREERTOS_DCP2_NOTIFY_H
#define BVSTK_FREERTOS_DCP2_NOTIFY_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

#include "shared/protocols/dcp2/bvstk_dcp2_model.h"

bool dcp2_notify_init(void);
bool dcp2_notify_receive(dcp2_notify_event_t *out, TickType_t timeout_ticks);
void dcp2_notify_publish(const dcp2_notify_event_t *event);
void dcp2_notify_publish_simple(uint16_t ev_type,
                                uint16_t status,
                                dcp2_notify_source_t source,
                                dcp2_notify_bus_t bus,
                                dcp2_notify_op_t op_kind,
                                uint32_t arg0,
                                uint32_t arg1,
                                uint32_t arg2);
uint32_t dcp2_notify_event_class_mask(uint16_t ev_type);

#endif /* BVSTK_FREERTOS_DCP2_NOTIFY_H */
