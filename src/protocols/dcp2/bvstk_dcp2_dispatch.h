#ifndef BVSTK_DCP2_DISPATCH_H
#define BVSTK_DCP2_DISPATCH_H

#include "protocols/dcp2/bvstk_dcp2_control.h"
#include "protocols/dcp2/bvstk_dcp2_policy.h"
#include "protocols/dcp2/bvstk_dcp2_cache.h"
#include "protocols/dcp2/bvstk_dcp2_fs.h"
#include "services/stream/bvstk_stream_sim.h"
#include "shared/events/bvstk_journal.h"

/* Only these callbacks know the application owner and its OS. A scope uses
 * bit 0 for I2C, bit 1 for MDIO; acquire in that order and release in reverse.
 * No transport I/O occurs while an application lock is held. */
typedef struct {
    void *context;
    bvstk_control_api_t control;
    bvstk_dcp2_policy_adapter_t policy;
    void (*lock)(void *, unsigned scope);
    void (*unlock)(void *, unsigned scope);
    bvstk_status_t (*sync)(void *, uint8_t service, size_t id, int capture_cache);
    bool i2c_supported, mdio_supported;
    const bvstk_fs_backend_t *files;
    bvstk_cache_registry_t *cache;
    uint64_t (*head)(void *, unsigned bus, unsigned channel);
    void (*read)(void *, unsigned bus, unsigned channel, uint64_t after,
                 bvstk_journal_batch_t *);
    bvstk_stream_sim_t *streams;
} bvstk_dcp2_adapter_t;

int bvstk_dcp2_dispatch(const bvstk_dcp2_adapter_t *,
    bvstk_dcp2_policy_session_t *, const bvstk_dcp2_request_t *, uint64_t now_ms,
    uint8_t *response, size_t capacity, size_t *response_size);

#endif
