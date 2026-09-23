#include "apps/freertos/services/http/http_ui.h"
#include "apps/freertos/runtime/bvstk_observer.h"
#include "shared/interfaces/bvstk_clock.h"
#include "shared/protocols/http/bvstk_http_input.h"
#include <string.h>

/* HTTP server is single-threaded. Snapshot first; never hold the journal's
 * critical section while formatting or waiting on a network client. */
static bvstk_journal_batch_t batch;

static int cursor_value(const char *text, uint64_t *value)
{
    uint64_t n = 0;
    if (!*text) return 0;
    for (; *text; ++text) {
        unsigned digit = (unsigned char)*text - '0';
        if (digit > 9U || n > (UINT64_MAX - digit) / 10U) return 0;
        n = n * 10U + digit;
    }
    if (n == BVSTK_JOURNAL_HISTORY) return 0;
    *value = n;
    return 1;
}
int http_ui_observe(const http_request_t *r, http_conn_t *c)
{
    char bus_text[12], channel_text[16], text[24];
    unsigned bus, channel;
    uint32_t address = 0;
    uint64_t after = BVSTK_JOURNAL_HISTORY;
    int field, filter = -1;
    size_t i;
    const size_t route_length = sizeof("/api/ui/observe") - 1U;
    if (strncmp(r->path, "/api/ui/observe", route_length) ||
        (r->path[route_length] && r->path[route_length] != '?')) return 0;
    if (strcmp(r->method, "GET")) { http_ui_error(c->fd, 405, "only GET"); return 1; }
    if (!http_ui_param(r, c, "bus", bus_text, sizeof(bus_text)) ||
        !http_ui_param(r, c, "channel", channel_text, sizeof(channel_text))) return 1;
    if (!strcmp(bus_text, "i2c")) bus = BVSTK_EVENT_BUS_I2C;
    else if (!strcmp(bus_text, "mdio")) bus = BVSTK_EVENT_BUS_MDIO;
    else { http_ui_error(c->fd, 400, "unsupported bus"); return 1; }
    if (!strcmp(channel_text, "telemetry")) channel = BVSTK_JOURNAL_TELEMETRY;
    else if (!strcmp(channel_text, "events")) channel = BVSTK_JOURNAL_EVENTS;
    else { http_ui_error(c->fd, 400, "unsupported channel"); return 1; }
    field = bvstk_http_query(r->path, "address", text, sizeof(text));
    if (field < 0 || (field && !bvstk_http_uint(text, bus == 0 ? 127 : 31, &address))) {
        http_ui_error(c->fd, 400, "invalid address"); return 1;
    }
    if (field) filter = (int)address;
    field = bvstk_http_query(r->path, "after", text, sizeof(text));
    if (field < 0 || (field && !cursor_value(text, &after))) {
        http_ui_error(c->fd, 400, "invalid cursor"); return 1;
    }
    bvstk_observer_read(bus, channel, filter, after, &batch);
    http_ui_header(c->fd, "application/json; charset=utf-8", -1);
    http_ui_printf(c->fd,
        "{\"ok\":true,\"next\":\"%llu\",\"head\":\"%llu\",\"lost\":\"%llu\",\"reset\":%s,\"more\":%s,\"capacity\":%u,\"uptime_ms\":%llu,\"records\":[",
        (unsigned long long)batch.next, (unsigned long long)batch.head,
        (unsigned long long)batch.lost, batch.reset ? "true" : "false",
        batch.next < batch.head ? "true" : "false", BVSTK_JOURNAL_CAPACITY,
        (unsigned long long)bvstk_platform_now_ms());
    for (i = 0; i < batch.count; ++i) {
        const bvstk_journal_record_t *record = &batch.records[i];
        const bvstk_event_t *e = &record->event;
        http_ui_printf(c->fd,
            "%s{\"id\":\"%llu\",\"time_us\":%llu,\"type\":%u,\"status\":%u,\"source\":%u,\"operation\":%u,\"address\":%lu,\"reg\":%lu,\"value\":%lu}",
            i ? "," : "", (unsigned long long)record->sequence,
            (unsigned long long)e->time_us, (unsigned)e->type, (unsigned)e->status,
            (unsigned)e->source, (unsigned)e->operation, (unsigned long)e->arg0,
            (unsigned long)e->arg1, (unsigned long)e->arg2);
    }
    http_ui_text(c->fd, "]}\n");
    return 1;
}
