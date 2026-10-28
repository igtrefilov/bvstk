#include "apps/neutrino/commands/bvstk_command_resmgr.h"
#include "apps/neutrino/runtime/bvstk_system_neutrino.h"
#include "apps/neutrino/http/bvstk_http_neutrino.h"
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

#include "apps/neutrino/i2c/bvstk_i2c_ipc.h"
#include "apps/neutrino/bvstkd/bvstk_dcp2_runtime.h"
#include "apps/neutrino/i2c/bvstk_i2c_resmgr.h"
#include "apps/neutrino/runtime/bvstk_i2c_runtime.h"
#include "protocols/dcp2/bvstk_dcp2_codec.h"
#include "protocols/dcp2/bvstk_dcp2_control.h"
#include "protocols/dcp2/bvstk_dcp2_policy.h"
#include "shared/interfaces/bvstk_clock.h"
#include "services/control/bvstk_control_api.h"
#include "shared/interfaces/bvstk_platform.h"
#include "shared/pl/access/bvstk_pl_service.h"
#include "ports/neutrino-zynq7000/os/bvstk_observation_neutrino.h"
#include "apps/neutrino/observe/bvstk_observe_resmgr.h"
#include "protocols/dcp2/bvstk_dcp2_session.h"
#include "ports/neutrino-zynq7000/storage/bvstk_fs_posix.h"

enum { BVSTKD_PORT = 8889 };

static bvstk_neutrino_i2c_runtime_t s_i2c_runtime;
static bvstk_i2c_resmgr_t s_i2c_resmgr;
static bvstk_neutrino_mdio_runtime_t s_mdio_runtime;
static bvstk_neutrino_dcp2_runtime_t s_runtime = {&s_i2c_runtime, &s_mdio_runtime};
static bvstk_dcp2_session_t s_session;
static bvstk_dcp2_adapter_t s_adapter;
static bvstk_neutrino_command_t s_commands;
static bvstk_command_resmgr_t s_command_resmgr;
static bvstk_stream_sim_t s_streams;
static pthread_mutex_t s_stream_mutex = PTHREAD_MUTEX_INITIALIZER;
static bvstk_observation_neutrino_t s_observation;
static bvstk_observe_resmgr_t s_observe_resmgr;
static void stream_enter(void *context) { (void)pthread_mutex_lock(context); }
static void stream_leave(void *context) { (void)pthread_mutex_unlock(context); }
static uint64_t observation_head(void *context, unsigned bus, unsigned channel)
{ (void)context; return bvstk_observation_head(&s_observation.events, bus, channel); }
static void observation_read(void *context, unsigned bus, unsigned channel,
    uint64_t after, bvstk_journal_batch_t *out)
{ (void)context; bvstk_observation_read(&s_observation.events, bus, channel, -1, -1, after, out); }


static int wait_socket(int fd, int writing, unsigned timeout_ms)
{
    fd_set set;
    struct timeval timeout = {(long)(timeout_ms / 1000U), (long)(timeout_ms % 1000U) * 1000L};
    FD_ZERO(&set); FD_SET(fd, &set);
    return select(fd + 1, writing ? NULL : &set, writing ? &set : NULL, NULL, &timeout);
}

static int read_exact(int fd, void *buffer, size_t size)
{
    size_t received = 0U;
    uint8_t *bytes = (uint8_t *)buffer;
    uint64_t progress = bvstk_platform_now_ms();

    while (received < size) {
        uint64_t elapsed = bvstk_platform_now_ms() - progress;
        if (elapsed >= 5000U || wait_socket(fd, 0, (unsigned)(5000U - elapsed)) <= 0) return -1;
        ssize_t result = recv(fd, bytes + received, size - received, MSG_DONTWAIT);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return -1;
        }
        received += (size_t)result;
        progress = bvstk_platform_now_ms();
    }
    return 0;
}

static int write_all(int fd, const void *buffer, size_t size)
{
    size_t sent = 0U;
    const uint8_t *bytes = (const uint8_t *)buffer;
    uint64_t progress = bvstk_platform_now_ms();

    while (sent < size) {
        uint64_t elapsed = bvstk_platform_now_ms() - progress;
        if (elapsed >= 15000U || wait_socket(fd, 1, (unsigned)(15000U - elapsed)) <= 0) return -1;
        ssize_t result = send(fd, bytes + sent, size - sent, MSG_DONTWAIT);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return -1;
        }
        sent += (size_t)result;
        progress = bvstk_platform_now_ms();
    }
    return 0;
}

static int send_frame(void *context, const void *data, size_t size)
{ return write_all(*(int *)context, data, size); }
static int serve_client(int fd, bvstk_control_api_t *control,
                        bvstk_neutrino_dcp2_runtime_t *runtime)
{
    static uint8_t frame[BVSTK_DCP2_HEADER_SIZE + BVSTK_DCP2_MAX_PAYLOAD];
    bvstk_neutrino_dcp2_adapter(&s_adapter, runtime, control);
    const bvstk_fs_storage_t *storage = bvstk_fs_posix_default();
    s_adapter.files = storage ? storage->reader : NULL;
    s_adapter.cache = s_observation.initialized ? &s_observation.cache : NULL;
    s_adapter.head = s_observation.initialized ? observation_head : NULL;
    s_adapter.read = s_observation.initialized ? observation_read : NULL;
    s_adapter.streams = &s_streams;
    bvstk_dcp2_session_init(&s_session, &s_adapter, send_frame, &fd);
    int result = 0;
    for (;;) {
        if (bvstk_dcp2_session_poll(&s_session, bvstk_platform_now_ms())) { result = -1; break; }
        int readable = wait_socket(fd, 0, 100U);
        if (readable < 0) { if (errno == EINTR) continue; result = -1; break; }
        if (!readable) continue;
        if (read_exact(fd, frame, BVSTK_DCP2_HEADER_SIZE)) break;
        size_t payload = bvstk_dcp2_read_be16(frame + 6U);
        if (memcmp(frame, "DCP2", 4U) || payload < BVSTK_DCP2_MIN_PAYLOAD ||
            payload > BVSTK_DCP2_MAX_PAYLOAD) { result = -1; break; }
        if (read_exact(fd, frame + 8U, payload) ||
            bvstk_dcp2_session_frame(&s_session, frame, payload + 8U, bvstk_platform_now_ms())) {
            result = -1; break;
        }
    }
    bvstk_dcp2_session_destroy(&s_session);
    return result;
}

static int create_server_socket(void)
{
    for (;;) {
        int server_fd;
        struct sockaddr_in address;
        int enabled = 1;

        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd >= 0) {
            (void)setsockopt(server_fd,
                             SOL_SOCKET,
                             SO_REUSEADDR,
                             &enabled,
                             sizeof(enabled));
            memset(&address, 0, sizeof(address));
            address.sin_family = AF_INET;
            address.sin_port = htons(BVSTKD_PORT);
            address.sin_addr.s_addr = htonl(INADDR_ANY);
            if (bind(server_fd,
                     (struct sockaddr *)&address,
                     sizeof(address)) == 0 &&
                listen(server_fd, 2) == 0) {
                return server_fd;
            }
            close(server_fd);
        }
        fprintf(stderr,
                "bvstkd: DCP2 socket unavailable, retrying: %s\n",
                strerror(errno));
        sleep(1U);
    }
}

int main(void)
{
    static bvstk_pl_service_t pl;
    bvstk_control_api_t control;
    int server_fd;
    /* A disconnected cache subscriber must not terminate the daemon. */
    (void)signal(SIGPIPE, SIG_IGN);

    if (bvstk_platform_init() != 0) {
        fprintf(stderr, "bvstkd: platform initialization failed\n");
    }
    bvstk_pl_service_init(&pl);
    const bvstk_guard_t stream_guard = {&s_stream_mutex, stream_enter, stream_leave};
    bvstk_stream_sim_init(&s_streams, &stream_guard);
    if (!bvstk_observation_neutrino_init(&s_observation))
        fprintf(stderr, "bvstkd: observation guards unavailable\n");
    bvstk_cache_registry_t *catalog = s_observation.initialized ? &s_observation.cache : NULL;
    bvstk_event_sink_t events = {&s_observation.events, bvstk_observation_publish};
    if (!bvstk_neutrino_i2c_runtime_init_observed(&s_i2c_runtime, catalog,
            s_observation.initialized ? &events : NULL)) {
        fprintf(stderr, "bvstkd: I2C unavailable; other services remain enabled\n");
    }
    if (!bvstk_i2c_resmgr_start(&s_i2c_resmgr, &s_i2c_runtime)) {
        fprintf(stderr,
                "bvstkd: failed to attach %s\n",
                BVSTK_I2C_DEVICE_PATH);
    }
    if (!bvstk_neutrino_mdio_runtime_init_observed(&s_mdio_runtime, catalog,
            s_observation.initialized ? &events : NULL))
        fprintf(stderr, "bvstkd: MDIO unavailable; other services remain enabled\n");
    if (s_observation.initialized &&
        !bvstk_observe_resmgr_start(&s_observe_resmgr, &s_observation.events, &s_runtime))
        fprintf(stderr, "bvstkd: observation resource unavailable\n");

    memset(&control, 0, sizeof(control));
    control.pl = &pl;
    control.i2c = bvstk_neutrino_i2c_runtime_service(&s_i2c_runtime);
    control.memory = bvstk_platform_memory();
    control.mdio = bvstk_neutrino_mdio_runtime_service(&s_mdio_runtime);

    s_commands = (bvstk_neutrino_command_t){&s_runtime, &control, &s_streams, bvstk_fs_posix_default(), bvstk_system_neutrino_command};
    (void)bvstk_system_neutrino_start(&s_commands);
    if (!bvstk_command_resmgr_start(&s_command_resmgr, &s_commands))
        fprintf(stderr, "bvstkd: control resource unavailable\n");

    (void)bvstk_http_neutrino_start(&s_commands,&s_observation.events);
    server_fd = create_server_socket();

    printf("bvstkd: I2C=%s (%u devices), MDIO=%s (%u devices), DCP2 port %u\n",
        control.i2c ? "ready" : "unavailable", (unsigned)s_i2c_runtime.config_store.device_count,
        control.mdio ? "ready" : "unavailable", (unsigned)s_mdio_runtime.config_store.device_count,
        BVSTKD_PORT);
    fflush(stdout);
    for (;;) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "bvstkd: accept failed: %s\n", strerror(errno));
            continue;
        }
        (void)serve_client(client_fd, &control, &s_runtime);
        close(client_fd);
    }
}
