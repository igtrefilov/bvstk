#ifndef BVSTK_NEUTRINO_OBSERVE_IPC_H
#define BVSTK_NEUTRINO_OBSERVE_IPC_H
#include <devctl.h>
#include "apps/neutrino/observe/bvstk_observe.h"

#define BVSTK_OBSERVE_DEVICE_PATH "/dev/bvstk-observe"
#define BVSTK_DCMD_OBSERVE __DIOTF(0x80, 2, bvstk_observe_message_t)
_Static_assert(sizeof(bvstk_observe_message_t) < 8192U, "bounded observation IPC");

#endif
