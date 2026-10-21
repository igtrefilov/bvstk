#ifndef BVSTK_ACQUISITION_MODEL_H
#define BVSTK_ACQUISITION_MODEL_H

#include <stdint.h>

#define BVSTK_ACQUISITION_GROUPS_MAX 8u
#define BVSTK_ACQUISITION_RANGES_MAX 4u
#define BVSTK_ACQUISITION_CONDITIONS_MAX 4u
#define BVSTK_ACQUISITION_CHUNK_MAX 16u

/* Resource indices are resolved from profile IDs when loading JSON. */
typedef struct { uint16_t first, count; } bvstk_acquisition_range_t;
typedef struct { uint16_t resource, mask, value; } bvstk_acquisition_condition_t;
typedef struct {
    char id[32];
    uint32_t interval_ms; /* 0: once per connected session; retry on failure */
    uint8_t chunk_size, range_count, condition_count;
    bvstk_acquisition_range_t ranges[BVSTK_ACQUISITION_RANGES_MAX];
    bvstk_acquisition_condition_t conditions[BVSTK_ACQUISITION_CONDITIONS_MAX];
} bvstk_acquisition_group_t;
typedef struct {
    uint8_t enabled, group_count;
    uint16_t read_delay_ms, retry_delay_ms;
    bvstk_acquisition_group_t groups[BVSTK_ACQUISITION_GROUPS_MAX];
} bvstk_acquisition_plan_t;

#endif
