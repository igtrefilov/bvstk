#ifndef BVSTK_NEUTRINO_I2C_RESMGR_H
#define BVSTK_NEUTRINO_I2C_RESMGR_H

#include <pthread.h>
#include <stdint.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>

#include "apps/neutrino/runtime/bvstk_i2c_runtime.h"

typedef struct {
    dispatch_t *dispatch;
    dispatch_context_t *context;
    pthread_t thread;
    /* resmgr_attach retains these tables for the manager lifetime. */
    resmgr_attr_t resmgr_attributes;
    resmgr_connect_funcs_t connect_functions;
    resmgr_io_funcs_t io_functions;
    iofunc_attr_t attribute;
    bvstk_neutrino_i2c_runtime_t *runtime;
    int attach_id;
    uint8_t initialized;
} bvstk_i2c_resmgr_t;

int bvstk_i2c_resmgr_start(bvstk_i2c_resmgr_t *manager,
                           bvstk_neutrino_i2c_runtime_t *runtime);

#endif /* BVSTK_NEUTRINO_I2C_RESMGR_H */
