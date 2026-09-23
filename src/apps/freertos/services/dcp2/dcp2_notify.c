#include "apps/freertos/services/dcp2/dcp2_notify.h"

uint32_t dcp2_notify_event_class_mask(uint16_t type)
{
    switch (type) {
    case DCP2_NOTIFY_EV_REG_ATTEMPT: return DCP2_NOTIFY_CLASS_REG_ATTEMPT;
    case DCP2_NOTIFY_EV_REG_COMMIT: return DCP2_NOTIFY_CLASS_REG_COMMIT;
    case DCP2_NOTIFY_EV_REG_DENIED: return DCP2_NOTIFY_CLASS_REG_DENIED;
    case DCP2_NOTIFY_EV_STATE_CHANGED: return DCP2_NOTIFY_CLASS_STATE_CHANGED;
    case DCP2_NOTIFY_EV_FAULT:
    case DCP2_NOTIFY_EV_LOST: return DCP2_NOTIFY_CLASS_FAULT;
    default: return 0;
    }
}
