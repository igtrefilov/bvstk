#ifndef BVSTK_NEUTRINO_COMMAND_RESMGR_H
#define BVSTK_NEUTRINO_COMMAND_RESMGR_H
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <pthread.h>
#include "apps/neutrino/commands/bvstk_command.h"
typedef struct {
    dispatch_t *dispatch;
    dispatch_context_t *context;
    resmgr_attr_t attributes;
    resmgr_connect_funcs_t connect;
    resmgr_io_funcs_t io;
    iofunc_attr_t node;
    iofunc_mount_t mount;
    iofunc_funcs_t functions;
    pthread_t thread;
    bvstk_neutrino_command_t *runtime;
    int attach_id;
} bvstk_command_resmgr_t;
int bvstk_command_resmgr_start(bvstk_command_resmgr_t *, bvstk_neutrino_command_t *);
#endif
