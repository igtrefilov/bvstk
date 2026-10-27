#ifndef BVSTK_MEMORY_H
#define BVSTK_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#include "shared/base/bvstk_status.h"

enum {
    BVSTK_MEMORY_8 = 1U,
    BVSTK_MEMORY_16 = 2U,
    BVSTK_MEMORY_32 = 4U,
    BVSTK_MEMORY_64 = 8U,
    BVSTK_MEMORY_ALL_WIDTHS = 15U
};

/* The owner serializes access to this backend. check() validates the complete
 * span without mapping memory or performing I/O. A 64-bit value is transferred
 * as the high 32-bit word followed by the low word, without atomicity. */
typedef struct {
    void *context;
    unsigned widths;
    bvstk_status_t (*check)(void *, uint32_t address, size_t span,
                            unsigned width, int writing);
    bvstk_status_t (*read)(void *, uint32_t address, unsigned width, uint64_t *);
    bvstk_status_t (*write)(void *, uint32_t address, unsigned width, uint64_t);
} bvstk_memory_t;

/* Process/application-lifetime backend; its allowed regions belong to the port. */
const bvstk_memory_t *bvstk_platform_memory(void);

#endif
