#ifndef BVSTK_DCP2_CONTROL_H
#define BVSTK_DCP2_CONTROL_H

#include <stddef.h>
#include <stdint.h>

#include "protocols/dcp2/bvstk_dcp2_codec.h"
#include "services/control/bvstk_control_api.h"

/* Shared RAM-only policy handler; output excludes the common status field.
 * NOT_FOUND and NOT_READY are normalized to RANGE and BUSY for DCP2. */
bvstk_status_t bvstk_dcp2_process_policy(
    bvstk_i2c_master_service_t *i2c, bvstk_mdio_service_t *mdio,
    const bvstk_dcp2_request_t *request, uint8_t *body,
    size_t body_capacity, uint16_t *body_size);

int bvstk_dcp2_process_request(bvstk_control_api_t *control,
                               const bvstk_dcp2_request_t *request,
                               uint8_t *response_frame,
                               size_t response_capacity,
                               size_t *response_size);

#endif
