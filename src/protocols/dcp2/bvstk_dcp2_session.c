#include "protocols/dcp2/bvstk_dcp2_session.h"
#include <string.h>

enum {
    DCP2_PORT_DEFAULT = 8889,
    DCP2_THREAD_STACK = 3072,
    DCP2_VER = 0x0002,
    DCP2_HDR_LEN = 8,
    DCP2_MAX_PAYLOAD = 4096,
    DCP2_MIN_PAYLOAD = 4,
};

enum {
    DCP2_SRV_PING = 0x00,
    DCP2_SRV_MEM = 0x01,
    DCP2_SRV_I2C = 0x02,
    DCP2_SRV_MDIO = 0x03,
    DCP2_SRV_SPI = 0x04,
    DCP2_SRV_UART = 0x05,
    DCP2_SRV_NOTIFY = 0x06,
    DCP2_SRV_FS = 0x07,
    DCP2_SRV_CACHE = 0x08,
    DCP2_SRV_POLICY = 0x09,
    DCP2_SRV_VENDOR = 0x7F,
};

enum {
    DCP2_OP_PING = 0x00,
    DCP2_OP_MEM_READ = 0x00,
    DCP2_OP_MEM_WRITE = 0x01,
    DCP2_OP_I2C_READ_REG = 0x00,
    DCP2_OP_I2C_WRITE_REG = 0x01,
    DCP2_OP_I2C_POLICY_SET = 0x02,
    DCP2_OP_MDIO_READ = 0x00,
    DCP2_OP_MDIO_WRITE = 0x01,
    DCP2_OP_NOTIFY_SUBSCRIBE = 0x10,
    DCP2_OP_NOTIFY_UNSUBSCRIBE = 0x11,
    DCP2_OP_PL_SUBSCRIBE_STREAM = 0x10,
    DCP2_OP_PL_UNSUBSCRIBE_STREAM = 0x11,
};

enum {
    DCP2_STATUS_OK = 0x0000,
    DCP2_STATUS_ERR_MALFORMED = 0x0001,
    DCP2_STATUS_ERR_UNSUPPORTED = 0x0002,
    DCP2_STATUS_ERR_DENIED = 0x0003,
    DCP2_STATUS_ERR_BUSY = 0x0004,
    DCP2_STATUS_ERR_TIMEOUT = 0x0005,
    DCP2_STATUS_ERR_RANGE = 0x0006,
    DCP2_STATUS_ERR_INTERNAL = 0x0007,
};

enum {
    DCP2_OP_RESP_BIT = 0x80,
    DCP2_OP_EVENT_BIT = 0x40,
    DCP2_OP_CODE_MASK = 0x3F,
};

enum {
    DCP2_MEM_FLAG_AUTOINC = 0x01,
    DCP2_SUB_RAW_WORDS = 0x01,
    DCP2_SUB_WITH_TIMESTAMP = 0x02,
    DCP2_SUB_RESET_LOST_COUNTERS = 0x04,
};

static uint16_t be16_read(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static uint32_t be32_read(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static void be16_write(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFFu);
}

static void be32_write(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)((v >> 16) & 0xFFu);
    p[2] = (uint8_t)((v >> 8) & 0xFFu);
    p[3] = (uint8_t)(v & 0xFFu);
}

static void be64_write(uint8_t *p, uint64_t v)
{
    be32_write(p, (uint32_t)(v >> 32));
    be32_write(p + 4, (uint32_t)(v & 0xFFFFFFFFu));
}

static int dcp2_send_response(bvstk_dcp2_session_t *session,
                              uint8_t srv,
                              uint8_t opcode,
                              uint16_t seq,
                              uint16_t status,
                              const uint8_t *body,
                              uint16_t body_len)
{
    uint16_t payload_len = (uint16_t)(1u + 1u + 2u + 2u + body_len);
    size_t frame_len = (size_t)DCP2_HDR_LEN + (size_t)payload_len;

    if ((size_t)payload_len > DCP2_MAX_PAYLOAD) return -1;

    session->tx[0] = 'D';
    session->tx[1] = 'C';
    session->tx[2] = 'P';
    session->tx[3] = '2';
    be16_write(session->tx + 4, DCP2_VER);
    be16_write(session->tx + 6, payload_len);
    session->tx[8] = srv;
    session->tx[9] = (uint8_t)(DCP2_OP_RESP_BIT | (opcode & DCP2_OP_CODE_MASK));
    be16_write(session->tx + 10, seq);
    be16_write(session->tx + 12, status);
    if (body_len && body) {
        memcpy(session->tx + 14, body, body_len);
    }
    return session->send(session->send_context, session->tx, frame_len);
}

static int dcp2_send_event(bvstk_dcp2_session_t *session, uint8_t srv, uint8_t opcode, const uint8_t *body, uint16_t body_len)
{
    uint16_t payload_len = (uint16_t)(1u + 1u + 2u + body_len);
    size_t frame_len = (size_t)DCP2_HDR_LEN + (size_t)payload_len;

    if ((size_t)payload_len > DCP2_MAX_PAYLOAD) return -1;

    session->tx[0] = 'D';
    session->tx[1] = 'C';
    session->tx[2] = 'P';
    session->tx[3] = '2';
    be16_write(session->tx + 4, DCP2_VER);
    be16_write(session->tx + 6, payload_len);
    session->tx[8] = srv;
    session->tx[9] = (uint8_t)(DCP2_OP_EVENT_BIT | (opcode & DCP2_OP_CODE_MASK));
    be16_write(session->tx + 10, 0u);
    if (body_len && body) {
        memcpy(session->tx + 12, body, body_len);
    }
    return session->send(session->send_context, session->tx, frame_len);
}

static uint32_t dcp2_notify_event_class_mask(uint16_t type)
{
    switch (type) {
    case DCP2_NOTIFY_EV_REG_ATTEMPT: return DCP2_NOTIFY_CLASS_REG_ATTEMPT;
    case DCP2_NOTIFY_EV_REG_COMMIT: return DCP2_NOTIFY_CLASS_REG_COMMIT;
    case DCP2_NOTIFY_EV_REG_DENIED: return DCP2_NOTIFY_CLASS_REG_DENIED;
    case DCP2_NOTIFY_EV_STATE_CHANGED: return DCP2_NOTIFY_CLASS_STATE_CHANGED;
    case DCP2_NOTIFY_EV_FAULT:
    case DCP2_NOTIFY_EV_LOST: return DCP2_NOTIFY_CLASS_FAULT;
    default: return 0;
    }
}

static bool dcp2_notify_filter_match(const bvstk_dcp2_connection_t *state, const dcp2_notify_event_t *event)
{
    uint32_t class_mask;
    uint32_t source_mask;
    uint32_t bus_mask;

    if (!state || !state->notify_enabled || !event) return false;
    class_mask = dcp2_notify_event_class_mask(event->ev_type);
    source_mask = (event->source < 32u) ? (1u << event->source) : 0u;
    bus_mask = (event->bus < 32u) ? (1u << event->bus) : 0u;

    if ((state->notify_filter.class_mask & class_mask) == 0u) return false;
    if ((state->notify_filter.source_mask & source_mask) == 0u) return false;
    if ((state->notify_filter.bus_mask & bus_mask) == 0u) return false;
    return true;
}

static int dcp2_send_notify_event(bvstk_dcp2_session_t *session, const bvstk_dcp2_connection_t *state, const dcp2_notify_event_t *event)
{
    uint8_t body[28];
    uint16_t body_len = 0u;

    if ((state->notify_filter.flags & DCP2_NOTIFY_FLAG_WITH_TIMESTAMP) != 0u) {
        be64_write(body + body_len, event->time_us);
        body_len = (uint16_t)(body_len + 8u);
    }
    be16_write(body + body_len, event->ev_type);
    body_len = (uint16_t)(body_len + 2u);
    be16_write(body + body_len, event->status);
    body_len = (uint16_t)(body_len + 2u);
    body[body_len++] = event->source;
    body[body_len++] = event->bus;
    body[body_len++] = event->op_kind;
    body[body_len++] = 0u;
    be32_write(body + body_len, event->arg0);
    body_len = (uint16_t)(body_len + 4u);
    be32_write(body + body_len, event->arg1);
    body_len = (uint16_t)(body_len + 4u);
    be32_write(body + body_len, event->arg2);
    body_len = (uint16_t)(body_len + 4u);

    return dcp2_send_event(session, DCP2_SRV_NOTIFY, DCP2_OP_NOTIFY_SUBSCRIBE, body, body_len);
}

static int dcp2_drain_notify_events(bvstk_dcp2_session_t *session, bvstk_dcp2_connection_t *state)
{
    unsigned bus;
    if (!state->notify_enabled || !session->adapter->read) return 0;
    for (bus = 0; bus < BVSTK_JOURNAL_BUSES; ++bus) {
        size_t i;
        if (!(state->notify_filter.bus_mask & (1U << bus))) continue;
        session->adapter->read(session->adapter->context, bus, BVSTK_JOURNAL_EVENTS,
            state->notify_cursor[bus], &session->observations);
        state->notify_cursor[bus] = session->observations.next;
        if (session->observations.lost) {
            /* Delivery metadata is visible even with a narrow event filter. */
            dcp2_notify_event_t lost = {0};
            lost.ev_type = DCP2_NOTIFY_EV_LOST;
            lost.time_us = (uint64_t)(uint32_t)session->now_ms * 1000U;
            lost.bus = (uint8_t)bus; lost.source = DCP2_NOTIFY_SOURCE_INTERNAL;
            lost.op_kind = DCP2_NOTIFY_OP_STATE_TOGGLE;
            lost.arg0 = session->observations.lost > UINT32_MAX ? UINT32_MAX : (uint32_t)session->observations.lost;
            if (dcp2_send_notify_event(session, state, &lost) < 0) return -1;
        }
        for (i = 0; i < session->observations.count; ++i) {
            const bvstk_event_t *e = &session->observations.records[i].event;
            dcp2_notify_event_t event = {0};
            event.time_us = e->time_us; event.ev_type = e->type;
            event.status = e->status; event.source = e->source;
            event.bus = e->bus; event.op_kind = e->operation;
            event.arg0 = e->arg0; event.arg1 = e->arg1; event.arg2 = e->arg2;
            if (!dcp2_notify_filter_match(state, &event)) continue;
            if (dcp2_send_notify_event(session, state, &event) < 0) return -1;
        }
    }
    return 0;
}

static int dcp2_stream_index(uint8_t srv);

static int dcp2_send_stream_event(bvstk_dcp2_session_t *session,
                                  const bvstk_dcp2_connection_t *state,
                                  const dcp2_stream_sim_event_t *event)
{
    uint8_t body[8u + 1u + 4u + 2u +
                 (DCP2_STREAM_SIM_MAX_WORDS * sizeof(uint32_t))];
    uint16_t body_len = 0u;
    int idx;

    if (state == NULL || event == NULL) return -1;
    idx = dcp2_stream_index(event->service);
    if (idx < 0 || !state->stream_enabled[idx]) return 0;
    if (event->data_len > sizeof(event->data) ||
        event->data_len > (uint16_t)(sizeof(body) - 15u)) {
        return -1;
    }

    if ((state->stream_flags[idx] & DCP2_SUB_WITH_TIMESTAMP) != 0u) {
        be64_write(body + body_len, event->time_us);
        body_len = (uint16_t)(body_len + 8u);
    }
    body[body_len++] = event->ev_flags;
    be32_write(body + body_len, event->lost_delta);
    body_len = (uint16_t)(body_len + 4u);
    be16_write(body + body_len, event->data_len);
    body_len = (uint16_t)(body_len + 2u);
    if (event->data_len != 0u) {
        memcpy(body + body_len, event->data, event->data_len);
        body_len = (uint16_t)(body_len + event->data_len);
    }

    return dcp2_send_event(session,
                           event->service,
                           DCP2_OP_PL_SUBSCRIBE_STREAM,
                           body,
                           body_len);
}

static int dcp2_drain_stream_events(bvstk_dcp2_session_t *session, bvstk_dcp2_connection_t *state)
{
    uint8_t srv;

    for (srv = DCP2_SRV_I2C; srv <= DCP2_SRV_UART; ++srv) {
        int idx = dcp2_stream_index(srv);
        dcp2_stream_sim_event_t event;

        if (idx < 0 || !state->stream_enabled[idx]) continue;
        if (bvstk_stream_sim_next(session->adapter->streams, (uint32_t)session->now_ms, srv, &event)) {
            event.ev_flags |= 0x04U; /* explicitly synthetic */
            if (dcp2_send_stream_event(session, state, &event) < 0) return -1;
        }
        if ((unsigned)idx < BVSTK_JOURNAL_BUSES && session->adapter->read) {
            size_t i;
            session->adapter->read(session->adapter->context, (unsigned)idx, BVSTK_JOURNAL_TELEMETRY,
                state->stream_cursor[idx], &session->observations);
            state->stream_cursor[idx] = session->observations.next;
            for (i = 0; i < session->observations.count; ++i) {
                const bvstk_journal_record_t *record = &session->observations.records[i];
                const bvstk_event_t *e = &record->event;
                memset(&event, 0, sizeof(event));
                event.service = srv; event.time_us = e->time_us;
                event.ev_flags = 0x02U; /* versioned software observation */
                if (i == 0 && session->observations.lost) {
                    event.ev_flags |= 0x01U;
                    event.lost_delta = session->observations.lost > UINT32_MAX ? UINT32_MAX : (uint32_t)session->observations.lost;
                }
                event.data_len = 32U;
                memcpy(event.data, "BVT1", 4);
                be64_write(event.data + 4, record->sequence);
                be16_write(event.data + 12, e->type);
                be16_write(event.data + 14, e->status);
                event.data[16] = e->source; event.data[17] = e->bus;
                event.data[18] = e->operation; event.data[19] = e->bus == 0 ? 8 : 16;
                be32_write(event.data + 20, e->arg0);
                be32_write(event.data + 24, e->arg1);
                be32_write(event.data + 28, e->arg2);
                if (dcp2_send_stream_event(session, state, &event) < 0) return -1;
            }
        }
    }
    return 0;
}

static int dcp2_stream_index(uint8_t srv)
{
    if (srv < DCP2_SRV_I2C || srv > DCP2_SRV_UART) return -1;
    return (int)(srv - DCP2_SRV_I2C);
}

static int dcp2_handle_notify(bvstk_dcp2_session_t *session,
                              bvstk_dcp2_connection_t *state,
                              uint8_t srv,
                              uint8_t opcode,
                              uint16_t seq,
                              const uint8_t *body,
                              uint16_t body_len)
{
    if (opcode == DCP2_OP_NOTIFY_SUBSCRIBE) {
        uint32_t class_mask;
        uint32_t source_mask;
        uint32_t bus_mask;
        uint8_t flags;

        if (body_len != 13u) {
            return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        }
        class_mask = be32_read(body + 0);
        source_mask = be32_read(body + 4);
        bus_mask = be32_read(body + 8);
        flags = body[12];

        if (class_mask == 0u || source_mask == 0u || bus_mask == 0u) {
            return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
        }
        if ((class_mask & ~((uint32_t)DCP2_NOTIFY_CLASS_REG_ATTEMPT |
                            (uint32_t)DCP2_NOTIFY_CLASS_REG_COMMIT |
                            (uint32_t)DCP2_NOTIFY_CLASS_REG_DENIED |
                            (uint32_t)DCP2_NOTIFY_CLASS_STATE_CHANGED |
                            (uint32_t)DCP2_NOTIFY_CLASS_FAULT)) != 0u) {
            return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
        }
        if ((source_mask & ~0x1Fu) != 0u || (bus_mask & ~0x1Fu) != 0u) {
            return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
        }
        if ((flags & (uint8_t)~(DCP2_NOTIFY_FLAG_WITH_TIMESTAMP | DCP2_NOTIFY_FLAG_SNAPSHOT_ON_SUBSCRIBE)) != 0u) {
            return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        }

        state->notify_enabled = true;
        state->notify_filter.class_mask = class_mask;
        state->notify_filter.source_mask = source_mask;
        state->notify_filter.bus_mask = bus_mask;
        state->notify_filter.flags = flags;
        for (unsigned b = 0; b < BVSTK_JOURNAL_BUSES; ++b)
            state->notify_cursor[b] = session->adapter->head ? session->adapter->head(session->adapter->context, b, BVSTK_JOURNAL_EVENTS) : 0;
        return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_OK, NULL, 0);
    }

    if (opcode == DCP2_OP_NOTIFY_UNSUBSCRIBE) {
        if (body_len != 0u) {
            return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        }
        memset(&state->notify_filter, 0, sizeof(state->notify_filter));
        state->notify_enabled = false;
        return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_OK, NULL, 0);
    }

    return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_ERR_UNSUPPORTED, NULL, 0);
}

static int dcp2_handle_stream_ctl(bvstk_dcp2_session_t *session,
                                  bvstk_dcp2_connection_t *state,
                                  uint8_t srv,
                                  uint8_t opcode,
                                  uint16_t seq,
                                  const uint8_t *body,
                                  uint16_t body_len)
{
    int idx = dcp2_stream_index(srv);
    if (idx < 0) {
        return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_ERR_UNSUPPORTED, NULL, 0);
    }

    if (opcode == DCP2_OP_PL_SUBSCRIBE_STREAM) {
        if (body_len != 1u) {
            return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        }
        if ((body[0] & (uint8_t)~(DCP2_SUB_RAW_WORDS | DCP2_SUB_WITH_TIMESTAMP | DCP2_SUB_RESET_LOST_COUNTERS)) != 0u) {
            return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        }
        state->stream_enabled[idx] = true;
        state->stream_flags[idx] = body[0];
        if ((unsigned)idx < BVSTK_JOURNAL_BUSES)
            state->stream_cursor[idx] = session->adapter->head ? session->adapter->head(session->adapter->context, (unsigned)idx, BVSTK_JOURNAL_TELEMETRY) : 0;
        if ((body[0] & DCP2_SUB_RESET_LOST_COUNTERS) != 0u) {
            bvstk_stream_sim_reset_lost(session->adapter->streams, (uint32_t)session->now_ms, srv);
        }
        return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_OK, NULL, 0);
    }

    if (opcode == DCP2_OP_PL_UNSUBSCRIBE_STREAM) {
        if (body_len != 0u) {
            return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        }
        state->stream_enabled[idx] = false;
        state->stream_flags[idx] = 0u;
        return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_OK, NULL, 0);
    }

    return dcp2_send_response(session, srv, opcode, seq, DCP2_STATUS_ERR_UNSUPPORTED, NULL, 0);
}


void bvstk_dcp2_session_init(bvstk_dcp2_session_t *s, const bvstk_dcp2_adapter_t *a,
    bvstk_dcp2_send_fn send, void *context)
{
    memset(s, 0, sizeof(*s));
    s->adapter = a; s->send = send; s->send_context = context;
    bvstk_dcp2_policy_init(&s->policy);
    bvstk_dcp2_cache_init(&s->cache, a->cache);
    bvstk_dcp2_fs_init(&s->files, a->files);
}
void bvstk_dcp2_session_destroy(bvstk_dcp2_session_t *s)
{
    if (!s) return;
    bvstk_dcp2_fs_destroy(&s->files);
    memset(s, 0, sizeof(*s));
}
int bvstk_dcp2_session_poll(bvstk_dcp2_session_t *s, uint64_t now)
{
    if (!s || !s->adapter || !s->send) return -1;
    s->now_ms = now;
    bvstk_dcp2_fs_expire(&s->files, (uint32_t)now);
    if (dcp2_drain_notify_events(s, &s->state) || dcp2_drain_stream_events(s, &s->state)) return -1;
    for (unsigned n = 0; s->cache.registry && n < 32U; ++n) {
        uint16_t size = bvstk_dcp2_cache_event(&s->cache, now, s->body, sizeof(s->body) - 4U);
        if (!size) break;
        if (dcp2_send_event(s, BVSTK_DCP2_SERVICE_CACHE, BVSTK_DCP2_CACHE_SUBSCRIBE, s->body, size)) return -1;
    }
    return 0;
}
int bvstk_dcp2_session_frame(bvstk_dcp2_session_t *s, const uint8_t *frame,
    size_t length, uint64_t now)
{
    if (!s || !s->adapter || !s->send || !frame || length < 12U ||
        memcmp(frame, "DCP2", 4U) || be16_read(frame + 4U) != BVSTK_DCP2_VERSION ||
        be16_read(frame + 6U) < 4U || be16_read(frame + 6U) > BVSTK_DCP2_MAX_PAYLOAD ||
        length != (size_t)be16_read(frame + 6U) + 8U) return -1;
    s->now_ms = now;
    uint8_t srv = frame[8], opcode = frame[9] & BVSTK_DCP2_OP_MASK;
    uint16_t seq = be16_read(frame + 10U);
    if (!seq || frame[9] != opcode)
        return dcp2_send_response(s, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
    bvstk_dcp2_request_t request;
    if (bvstk_dcp2_decode_request(frame, length, &request)) return -1;
    if (srv == BVSTK_DCP2_SERVICE_NOTIFY) {
        if (!s->adapter->head || !s->adapter->read)
            return dcp2_send_response(s, srv, opcode, seq, DCP2_STATUS_ERR_BUSY, NULL, 0);
        return dcp2_handle_notify(s, &s->state, srv, opcode, seq, request.body, request.body_size);
    }
    if (srv >= BVSTK_DCP2_SERVICE_I2C && srv <= BVSTK_DCP2_SERVICE_UART &&
        (opcode == DCP2_OP_PL_SUBSCRIBE_STREAM || opcode == DCP2_OP_PL_UNSUBSCRIBE_STREAM))
        return dcp2_handle_stream_ctl(s, &s->state, srv, opcode, seq, request.body, request.body_size);
    uint16_t size = 0, status;
    if (srv == BVSTK_DCP2_SERVICE_FS) {
        status = s->adapter->files ? bvstk_dcp2_fs_request(&s->files, opcode,
            request.body, request.body_size, (uint32_t)now, s->body, sizeof(s->body) - 6U, &size) :
            DCP2_STATUS_ERR_UNSUPPORTED;
    } else if (srv == BVSTK_DCP2_SERVICE_CACHE) {
        status = s->cache.registry ? bvstk_dcp2_cache_request(&s->cache, opcode,
            request.body, request.body_size, now, s->body, sizeof(s->body) - 6U, &size) :
            DCP2_STATUS_ERR_BUSY;
    } else {
        size_t response_size;
        if (bvstk_dcp2_dispatch(s->adapter, &s->policy, &request, now,
            s->tx, sizeof(s->tx), &response_size)) return -1;
        return s->send(s->send_context, s->tx, response_size);
    }
    return dcp2_send_response(s, srv, opcode, seq, status,
        status ? NULL : s->body, status ? 0U : size);
}
