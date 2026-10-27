#ifndef BVSTK_NEUTRINO_OBSERVE_RESMGR_H
#define BVSTK_NEUTRINO_OBSERVE_RESMGR_H
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include "apps/neutrino/observe/bvstk_observe.h"

typedef struct {
    dispatch_t *dispatch;
    dispatch_context_t *context;
    resmgr_attr_t attributes;
    resmgr_connect_funcs_t connect;
    resmgr_io_funcs_t io;
    iofunc_attr_t node;
    pthread_t thread;
    bvstk_observation_t *observer;
    bvstk_neutrino_dcp2_runtime_t *runtime;
    int attach_id;
} bvstk_observe_resmgr_t;
int bvstk_observe_resmgr_start(bvstk_observe_resmgr_t *, bvstk_observation_t *,
    bvstk_neutrino_dcp2_runtime_t *);

#endif
