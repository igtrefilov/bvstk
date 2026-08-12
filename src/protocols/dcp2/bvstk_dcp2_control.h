#ifndef BVSTK_DCP2_CONTROL_H
#define BVSTK_DCP2_CONTROL_H

#include <stddef.h>
#include <stdint.h>

#include "protocols/dcp2/bvstk_dcp2_codec.h"
#include "services/control/bvstk_control_api.h"

int bvstk_dcp2_process_request(bvstk_control_api_t *control,
                               const bvstk_dcp2_request_t *request,
                               uint8_t *response_frame,
                               size_t response_capacity,
                               size_t *response_size);

#endif
