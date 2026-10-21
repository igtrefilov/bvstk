#ifndef BVSTK_ACQUISITION_H
#define BVSTK_ACQUISITION_H

#include "shared/config/bvstk_acquisition_model.h"
#include "shared/base/bvstk_status.h"

/* One owner calls step(). Backends provide synchronized access to their cache.
 * The engine has no dependency on a bus, OS or device model. */
typedef struct {
    void *context;
    int (*ready)(void *);
    int (*cached)(void *, unsigned resource, uint16_t *value);
    bvstk_status_t (*read)(void *, unsigned first, unsigned count);
    void (*invalidate)(void *, unsigned first, unsigned count);
} bvstk_acquisition_io_t;
typedef struct {
    uint64_t due;
    uint16_t offset;
    uint8_t range, complete;
} bvstk_acquisition_group_state_t;
typedef struct {
    bvstk_acquisition_group_state_t groups[BVSTK_ACQUISITION_GROUPS_MAX];
    uint64_t due;
    uint8_t next_group, connected;
} bvstk_acquisition_state_t;

/* At most one bounded read per call. Returns 1 if a read was attempted. */
int bvstk_acquisition_step(const bvstk_acquisition_plan_t *,
    bvstk_acquisition_state_t *, const bvstk_acquisition_io_t *, uint64_t now_ms);

#endif
