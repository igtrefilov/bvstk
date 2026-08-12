#ifndef BVSTK_SHARED_IRQ_H
#define BVSTK_SHARED_IRQ_H

#include <stdint.h>

#include "shared/base/bvstk_status.h"

typedef struct {
    void *context;
    bvstk_status_t (*attach)(void *context, uint32_t irq, void *handler, void *arg);
    void (*detach)(void *context, uint32_t irq);
    bvstk_status_t (*enable)(void *context, uint32_t irq);
    void (*disable)(void *context, uint32_t irq);
} bvstk_irq_controller_t;

#endif
