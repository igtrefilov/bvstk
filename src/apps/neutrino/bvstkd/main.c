#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "apps/neutrino/i2c/bvstk_i2c_ipc.h"
#include "apps/neutrino/i2c/bvstk_i2c_resmgr.h"
#include "apps/neutrino/runtime/bvstk_i2c_runtime.h"
#include "protocols/dcp2/bvstk_dcp2_codec.h"
#include "protocols/dcp2/bvstk_dcp2_control.h"
#include "services/control/bvstk_control_api.h"
#include "shared/interfaces/bvstk_platform.h"
#include "shared/pl/access/bvstk_pl_service.h"

enum { BVSTKD_PORT = 8889 };

static bvstk_neutrino_i2c_runtime_t s_i2c_runtime;
static bvstk_i2c_resmgr_t s_i2c_resmgr;

static int read_exact(int fd, void *buffer, size_t size)
{
    size_t received = 0U;
    uint8_t *bytes = (uint8_t *)buffer;

    while (received < size) {
        ssize_t result = recv(fd, bytes + received, size - received, 0);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return -1;
        }
        received += (size_t)result;
    }
    return 0;
}

static int write_all(int fd, const void *buffer, size_t size)
{
    size_t sent = 0U;
    const uint8_t *bytes = (const uint8_t *)buffer;

    while (sent < size) {
        ssize_t result = send(fd, bytes + sent, size - sent, 0);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return -1;
        }
        sent += (size_t)result;
    }
    return 0;
}

static int request_mutates_i2c(const bvstk_dcp2_request_t *request)
{
    return request != NULL &&
           request->service == BVSTK_DCP2_SERVICE_I2C &&
           (request->opcode == BVSTK_DCP2_OP_I2C_WRITE_REG ||
            request->opcode == BVSTK_DCP2_OP_I2C_POLICY_SET);
}

static void persist_dcp_i2c_mutation(
    bvstk_neutrino_i2c_runtime_t *runtime,
    const bvstk_dcp2_request_t *request,
    const uint8_t *response,
    size_t response_size)
{
    size_t device_id;

    if (!request_mutates_i2c(request) || request->body_size == 0U ||
        response == NULL || response_size < 14U ||
        bvstk_dcp2_read_be16(response + 12U) != 0U ||
        bvstk_i2c_master_service_find_by_addr(
            bvstk_neutrino_i2c_runtime_service(runtime),
            request->body[0],
            &device_id) != BVSTK_OK) {
        return;
    }
    if (!bvstk_neutrino_i2c_runtime_sync_locked(runtime, device_id, 1)) {
        fprintf(stderr,
                "bvstkd: failed to persist DCP2 I2C mutation for address "
                "0x%02x\n",
                (unsigned)request->body[0]);
    }
}

static int serve_client(int fd,
                        bvstk_control_api_t *control,
                        bvstk_neutrino_i2c_runtime_t *runtime)
{
    uint8_t header[BVSTK_DCP2_HEADER_SIZE];
    uint8_t request_frame[BVSTK_DCP2_HEADER_SIZE + BVSTK_DCP2_MAX_PAYLOAD];
    uint8_t response_frame[BVSTK_DCP2_HEADER_SIZE + BVSTK_DCP2_MAX_PAYLOAD];

    for (;;) {
        uint16_t payload_size;
        size_t request_size;
        size_t response_size = 0U;
        bvstk_dcp2_request_t request;
        int process_result;

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

        bvstk_neutrino_i2c_runtime_lock(runtime);
        process_result = bvstk_dcp2_process_request(control,
                                                     &request,
                                                     response_frame,
                                                     sizeof(response_frame),
                                                     &response_size);
        if (process_result == 0) {
            persist_dcp_i2c_mutation(runtime,
                                     &request,
                                     response_frame,
                                     response_size);
        }
        bvstk_neutrino_i2c_runtime_unlock(runtime);
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

    if (bvstk_platform_init() != 0) {
        fprintf(stderr, "bvstkd: platform initialization failed\n");
        return 1;
    }
    bvstk_pl_service_init(&pl);
    if (!bvstk_neutrino_i2c_runtime_init(&s_i2c_runtime)) {
        fprintf(stderr, "bvstkd: I2C runtime initialization failed\n");
        return 1;
    }
    if (!bvstk_i2c_resmgr_start(&s_i2c_resmgr, &s_i2c_runtime)) {
        fprintf(stderr,
                "bvstkd: failed to attach %s\n",
                BVSTK_I2C_DEVICE_PATH);
        return 1;
    }

    memset(&control, 0, sizeof(control));
    control.pl = &pl;
    control.i2c = bvstk_neutrino_i2c_runtime_service(&s_i2c_runtime);

    server_fd = create_server_socket();

    printf("bvstkd: I2C runtime ready (%u device%s), DCP2 port %u\n",
           (unsigned)s_i2c_runtime.config_store.device_count,
           s_i2c_runtime.config_store.device_count == 1U ? "" : "s",
           BVSTKD_PORT);
    for (;;) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "bvstkd: accept failed: %s\n", strerror(errno));
            continue;
        }
        (void)serve_client(client_fd, &control, &s_i2c_runtime);
        close(client_fd);
    }
}
