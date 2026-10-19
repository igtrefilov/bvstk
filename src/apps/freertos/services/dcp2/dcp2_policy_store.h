#ifndef BVSTK_FREERTOS_DCP2_POLICY_STORE_H
#define BVSTK_FREERTOS_DCP2_POLICY_STORE_H

#include "protocols/dcp2/bvstk_dcp2_control.h"

/* POLICY_SAVE adapter. Writes one profile through the normal configuration store. */
bvstk_status_t bvstk_freertos_dcp2_policy_save(
    void *context, uint8_t service, size_t device_id);

#endif
