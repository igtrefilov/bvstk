#ifndef BVSTK_DCP2_SESSION_H
#define BVSTK_DCP2_SESSION_H
#include "protocols/dcp2/bvstk_dcp2_dispatch.h"
#include "shared/protocols/dcp2/bvstk_dcp2_model.h"

typedef struct {
    bool stream_enabled[4];
    uint8_t stream_flags[4];
    bool notify_enabled;
    dcp2_notify_filter_t notify_filter;
    uint64_t notify_cursor[BVSTK_JOURNAL_BUSES];
    uint64_t stream_cursor[BVSTK_JOURNAL_BUSES];
} bvstk_dcp2_connection_t;

typedef int (*bvstk_dcp2_send_fn)(void *, const void *, size_t);
/* Allocate in static storage or on the heap, never on an embedded task stack.
 * One instance per connection; destroy on every disconnect/error. */
typedef struct {
    const bvstk_dcp2_adapter_t *adapter;
    bvstk_dcp2_send_fn send;
    void *send_context;
    uint64_t now_ms;
    bvstk_dcp2_connection_t state;
    bvstk_dcp2_fs_session_t files;
    bvstk_dcp2_cache_session_t cache;
    bvstk_dcp2_policy_session_t policy;
    bvstk_journal_batch_t observations;
    uint8_t tx[BVSTK_DCP2_HEADER_SIZE + BVSTK_DCP2_MAX_PAYLOAD];
    uint8_t body[BVSTK_DCP2_MAX_PAYLOAD];
} bvstk_dcp2_session_t;
void bvstk_dcp2_session_init(bvstk_dcp2_session_t *, const bvstk_dcp2_adapter_t *,
    bvstk_dcp2_send_fn, void *);
void bvstk_dcp2_session_destroy(bvstk_dcp2_session_t *);
/* Calls send only outside all service/catalog/journal guards. */
int bvstk_dcp2_session_frame(bvstk_dcp2_session_t *, const uint8_t *, size_t, uint64_t now_ms);
int bvstk_dcp2_session_poll(bvstk_dcp2_session_t *, uint64_t now_ms);
#endif
