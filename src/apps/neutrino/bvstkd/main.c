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
#include "protocols/dcp2/bvstk_dcp2_cache.h"

enum { BVSTKD_PORT = 8889 };

static bvstk_neutrino_i2c_runtime_t s_i2c_runtime;
static bvstk_i2c_resmgr_t s_i2c_resmgr;
static bvstk_neutrino_mdio_runtime_t s_mdio_runtime;
static bvstk_neutrino_dcp2_runtime_t s_runtime = {&s_i2c_runtime, &s_mdio_runtime};
static bvstk_dcp2_policy_session_t s_policy_session;
static bvstk_observation_neutrino_t s_observation;
static bvstk_observe_resmgr_t s_observe_resmgr;
static bvstk_dcp2_cache_session_t s_cache_session;

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

static int serve_client(int fd,
                        bvstk_control_api_t *control,
                        bvstk_neutrino_dcp2_runtime_t *runtime)
{
    uint8_t header[BVSTK_DCP2_HEADER_SIZE];
    uint8_t request_frame[BVSTK_DCP2_HEADER_SIZE + BVSTK_DCP2_MAX_PAYLOAD];
    uint8_t response_frame[BVSTK_DCP2_HEADER_SIZE + BVSTK_DCP2_MAX_PAYLOAD];
    bvstk_dcp2_policy_init(&s_policy_session);
    bvstk_dcp2_cache_init(&s_cache_session, s_observation.initialized ? &s_observation.cache : NULL);

    for (;;) {
        uint16_t payload_size;
        size_t request_size;
        size_t response_size = 0U;
        bvstk_dcp2_request_t request;
        int process_result;

        /* Cache subscribers receive changes even when they send no requests.
         * Encoding and socket I/O occur after the catalog guard is released. */
        for (unsigned n = 0; s_cache_session.registry && n < 32U; ++n) {
            uint16_t size = bvstk_dcp2_cache_event(&s_cache_session, bvstk_platform_now_ms(),
                response_frame + 12U, sizeof(response_frame) - 12U);
            if (!size) break;
            memcpy(response_frame, "DCP2", 4U);
            bvstk_dcp2_write_be16(response_frame + 4U, BVSTK_DCP2_VERSION);
            bvstk_dcp2_write_be16(response_frame + 6U, (uint16_t)(size + 4U));
            response_frame[8] = BVSTK_DCP2_SERVICE_CACHE;
            response_frame[9] = BVSTK_DCP2_CACHE_SUBSCRIBE | BVSTK_DCP2_OP_EVENT;
            bvstk_dcp2_write_be16(response_frame + 10U, 0U);
            if (write_all(fd, response_frame, size + 12U)) return -1;
        }
        int readable = wait_socket(fd, 0, 100U);
        if (readable < 0) { if (errno == EINTR) continue; return -1; }
        if (!readable) continue;

        if (read_exact(fd, header, sizeof(header)) != 0) {
            return 0;
        }
        if (header[0] != 'D' || header[1] != 'C' ||
            header[2] != 'P' || header[3] != '2') {
            return -1;
        }
        payload_size = bvstk_dcp2_read_be16(header + 6U);
        if (payload_size < BVSTK_DCP2_MIN_PAYLOAD ||
            payload_size > BVSTK_DCP2_MAX_PAYLOAD) {
            return -1;
        }
        request_size = BVSTK_DCP2_HEADER_SIZE + payload_size;
        memcpy(request_frame, header, sizeof(header));
        if (read_exact(fd,
                       request_frame + BVSTK_DCP2_HEADER_SIZE,
                       payload_size) != 0) {
            return 0;
        }
        if (bvstk_dcp2_decode_request(request_frame,
                                      request_size,
                                      &request) != 0) {
            return -1;
        }

        if (request.service == BVSTK_DCP2_SERVICE_CACHE) {
            uint8_t body[BVSTK_DCP2_MAX_PAYLOAD - 6U];
            uint16_t size = 0;
            uint16_t status = s_cache_session.registry ?
                bvstk_dcp2_cache_request(&s_cache_session, request.opcode, request.body,
                    request.body_size, bvstk_platform_now_ms(), body, sizeof(body), &size) :
                bvstk_dcp2_status_from_bvstk(BVSTK_ERR_NOT_READY);
            process_result = bvstk_dcp2_encode_response(response_frame, sizeof(response_frame),
                &request, status, status ? NULL : body, status ? 0U : size, &response_size);
        } else {
            process_result = bvstk_neutrino_dcp2_dispatch(runtime, &s_policy_session, control,
                &request, response_frame, sizeof(response_frame), &response_size);
        }
        if (process_result != 0 ||
            write_all(fd, response_frame, response_size) != 0) {
            return -1;
        }
    }
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
