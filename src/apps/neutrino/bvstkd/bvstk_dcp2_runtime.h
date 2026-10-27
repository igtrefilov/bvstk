#ifndef BVSTK_NEUTRINO_DCP2_RUNTIME_H
#define BVSTK_NEUTRINO_DCP2_RUNTIME_H

#include "apps/neutrino/runtime/bvstk_i2c_runtime.h"
#include "protocols/dcp2/bvstk_dcp2_control.h"
#include "protocols/dcp2/bvstk_dcp2_policy.h"
#include "apps/neutrino/runtime/bvstk_mdio_runtime.h"

typedef struct {
    bvstk_neutrino_i2c_runtime_t *i2c;
    bvstk_neutrino_mdio_runtime_t *mdio;
} bvstk_neutrino_dcp2_runtime_t;

/* Locks only the requested subsystem; POLICY takes I2C then MDIO. Network I/O
 * stays outside the locks. Both callbacks and legacy commands share RAM/SAVE. */
int bvstk_neutrino_dcp2_dispatch(bvstk_neutrino_dcp2_runtime_t *,
    bvstk_dcp2_policy_session_t *, bvstk_control_api_t *,
    const bvstk_dcp2_request_t *, uint8_t *, size_t, size_t *);

/* The bvstkd owner holds the runtime state mutex. Register commands and RAM
 * mirroring are one serialized request; persistence is a separate SAVE. */
int bvstk_neutrino_dcp2_request_locked(bvstk_neutrino_i2c_runtime_t *runtime,
    bvstk_control_api_t *control, const bvstk_dcp2_request_t *request,
    uint8_t *response, size_t capacity, size_t *response_size);

#endif
