#include "apps/neutrino/observe/bvstk_observe_client.h"
#include "apps/neutrino/observe/bvstk_observe_ipc.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int arguments(int argc, char **argv, bvstk_observe_message_t *m)
{
    if (argc < 2 || argc > 4) return 0;
    if (!strcmp(argv[0], "i2c")) m->bus = BVSTK_EVENT_BUS_I2C;
    else if (!strcmp(argv[0], "mdio")) m->bus = BVSTK_EVENT_BUS_MDIO;
    else return 0;
    if (!strcmp(argv[1], "events")) m->channel = BVSTK_JOURNAL_EVENTS;
    else if (!strcmp(argv[1], "telemetry")) m->channel = BVSTK_JOURNAL_TELEMETRY;
    else return 0;
    m->version = BVSTK_OBSERVE_VERSION;
    m->address = -1;
    m->after = BVSTK_JOURNAL_HISTORY;
    if (argc >= 3 && strcmp(argv[2], "history")) {
        char *end;
        if (!argv[2][0] || argv[2][0] < '0' || argv[2][0] > '9') return 0;
        errno = 0;
        m->after = strtoull(argv[2], &end, 10);
        if (errno || *end || m->after == BVSTK_JOURNAL_HISTORY) return 0;
    }
    if (argc == 4) {
        if (!argv[3][0] || strlen(argv[3]) >= sizeof(m->device)) return 0;
        strcpy(m->device, argv[3]);
    }
    return 1;
}
int bvstk_observe_client_execute(int argc, char **argv)
{
    bvstk_observe_message_t m;
    memset(&m, 0, sizeof(m));
    if (!arguments(argc, argv, &m)) {
        fprintf(stderr, "Usage: bvstkctl observe <i2c|mdio> <events|telemetry> [after|history] [device]\n");
        return 2;
    }
    int fd = open(BVSTK_OBSERVE_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "bvstkctl: cannot open %s: %s\n", BVSTK_OBSERVE_DEVICE_PATH, strerror(errno));
        return 1;
    }
    int status = devctl(fd, BVSTK_DCMD_OBSERVE, &m, sizeof(m), NULL);
    close(fd);
    if (status || m.status != BVSTK_OK || m.version != BVSTK_OBSERVE_VERSION ||
        m.batch.count > BVSTK_JOURNAL_BATCH) {
        fprintf(stderr, "bvstkctl: observe failed: %s\n", status ? strerror(status) :
            bvstk_status_string((bvstk_status_t)m.status));
        return 1;
    }
    printf("{\"ok\":true,\"next\":\"%" PRIu64 "\",\"head\":\"%" PRIu64
        "\",\"lost\":\"%" PRIu64 "\",\"reset\":%s,\"more\":%s,\"capacity\":%u,"
        "\"uptime_ms\":%" PRIu64 ",\"records\":[", m.batch.next, m.batch.head, m.batch.lost,
        m.batch.reset ? "true" : "false", m.batch.next < m.batch.head ? "true" : "false",
        BVSTK_JOURNAL_CAPACITY, m.uptime_ms);
    for (size_t i = 0; i < m.batch.count; ++i) {
        const bvstk_journal_record_t *r = &m.batch.records[i];
        const bvstk_event_t *e = &r->event;
        printf("%s{\"id\":\"%" PRIu64 "\",\"time_us\":%" PRIu64 ",\"type\":%u,"
            "\"status\":%u,\"source\":%u,\"operation\":%u,\"address\":%" PRIu32
            ",\"reg\":%" PRIu32 ",\"value\":%" PRIu32 "}", i ? "," : "", r->sequence,
            e->time_us, (unsigned)e->type, (unsigned)e->status, (unsigned)e->source,
            (unsigned)e->operation, e->arg0, e->arg1, e->arg2);
    }
    puts("]}");
    return 0;
}
