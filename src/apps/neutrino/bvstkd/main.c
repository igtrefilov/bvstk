#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "drivers/pl/i2c/bvstk_i2c_master.h"
#include "drivers/pl/smi/bvstk_smi_core.h"
#include "drivers/pl/spi/bvstk_spi_core.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "hardware/pl/spi/bvstk_spi_regs.h"
#include "protocols/dcp2/bvstk_dcp2_codec.h"
#include "protocols/dcp2/bvstk_dcp2_control.h"
#include "services/control/bvstk_control_api.h"
#include "services/i2c/bvstk_i2c_master_service.h"
#include "services/smi/bvstk_smi_service.h"
#include "shared/base/bvstk_status.h"
#include "shared/config/bvstk_config_model.h"
#include "shared/interfaces/bvstk_platform.h"
#include "shared/pl/access/bvstk_pl_service.h"

enum { BVSTKD_PORT = 8889 };

static int read_exact(int fd, void *buffer, size_t size)
{
    size_t received = 0U;
    uint8_t *bytes = (uint8_t *)buffer;

    while (received < size) {
        ssize_t result = recv(fd, bytes + received, size - received, 0);
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
        if (result <= 0) {
            return -1;
        }
        sent += (size_t)result;
    }
    return 0;
}

static int serve_client(int fd, bvstk_control_api_t *control)
{
    uint8_t header[BVSTK_DCP2_HEADER_SIZE];
    uint8_t request_frame[BVSTK_DCP2_HEADER_SIZE + BVSTK_DCP2_MAX_PAYLOAD];
    uint8_t response_frame[BVSTK_DCP2_HEADER_SIZE + BVSTK_DCP2_MAX_PAYLOAD];

    for (;;) {
        uint16_t payload_size;
        size_t request_size;
        size_t response_size = 0U;
        bvstk_dcp2_request_t request;

        if (read_exact(fd, header, sizeof(header)) != 0) {
            return 0;
        }
        if (header[0] != 'D' || header[1] != 'C' ||
            header[2] != 'P' || header[3] != '2') {
            return -1;
        }
        payload_size = bvstk_dcp2_read_be16(header + 6);
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
        if (bvstk_dcp2_process_request(control,
                                       &request,
                                       response_frame,
                                       sizeof(response_frame),
                                       &response_size) != 0 ||
            write_all(fd, response_frame, response_size) != 0) {
            return -1;
        }
    }
}

int main(void)
{
    static const i2c_device_config_t default_device = {
        .name = "default",
        .file_name = "default.json",
        .addr_7b = 0x50,
        .reg_count = 256,
        .max_value_code = 64,
        .policy = I2C_POLICY_BLACKLIST
    };
    static const smi_phy_config_t default_phy = {
        .name = "default",
        .file_name = "default.json",
        .phy_addr = 1,
        .reg_count = 32,
        .policy = SMI_POLICY_BLACKLIST,
        .autopoll_enabled = false
    };
    static const bvstk_spi_core_config_t default_spi_config = {
        .packets_mode = BVSTK_SPI_MODE_MULTI,
        .timeout_ticks = 1,
        .p_clk_div = 512,
        .read_enable = true
    };
    bvstk_pl_service_t pl;
    bvstk_i2c_master_hw_t i2c_hardware;
    bvstk_i2c_devices_t i2c_devices;
    bvstk_i2c_cache_t i2c_cache;
    bvstk_i2c_policy_t i2c_policy;
    bvstk_i2c_master_service_t i2c;
    bvstk_smi_core_t smi_core;
    bvstk_smi_service_t smi;
    bvstk_spi_core_t spi;
    bvstk_control_api_t control;
    int server_fd;
    struct sockaddr_in address;
    bvstk_status_t status;

    if (bvstk_platform_init() != 0) {
        fprintf(stderr, "bvstkd: platform initialization failed\n");
        return 1;
    }
    bvstk_pl_service_init(&pl);
    status = bvstk_i2c_devices_init_from_config(&i2c_devices,
                                                &default_device,
                                                1U);
    if (status == BVSTK_OK) {
        status = bvstk_i2c_cache_init(&i2c_cache, 1U);
    }
    if (status == BVSTK_OK) {
        status = bvstk_i2c_policy_init(&i2c_policy, &default_device, 1U);
    }
    if (status == BVSTK_OK) {
        status = bvstk_i2c_master_hw_init(&i2c_hardware, NULL, NULL);
    }
    if (status != BVSTK_OK) {
        fprintf(stderr, "bvstkd: I2C initialization failed: %s\n",
                bvstk_status_string(status));
        return 1;
    }
    status = bvstk_i2c_master_service_init(&i2c,
                                           &i2c_hardware,
                                           NULL,
                                           &i2c_devices,
                                           &i2c_cache,
                                           &i2c_policy,
                                           NULL);
    if (status != BVSTK_OK) {
        fprintf(stderr, "bvstkd: I2C service initialization failed: %s\n",
                bvstk_status_string(status));
        bvstk_i2c_master_hw_shutdown(&i2c_hardware);
        bvstk_i2c_policy_shutdown(&i2c_policy);
        bvstk_i2c_cache_shutdown(&i2c_cache);
        bvstk_i2c_devices_shutdown(&i2c_devices);
        return 1;
    }
    status = bvstk_smi_core_init(&smi_core, NULL, NULL);
    if (status != BVSTK_OK) {
        fprintf(stderr, "bvstkd: SMI initialization failed: %s\n",
                bvstk_status_string(status));
        bvstk_i2c_master_service_shutdown(&i2c);
        bvstk_i2c_master_hw_shutdown(&i2c_hardware);
        bvstk_i2c_policy_shutdown(&i2c_policy);
        bvstk_i2c_cache_shutdown(&i2c_cache);
        bvstk_i2c_devices_shutdown(&i2c_devices);
        return 1;
    }
    status = bvstk_smi_service_init(&smi,
                                    &smi_core,
                                    NULL,
                                    &default_phy,
                                    1U,
                                    NULL);
    if (status != BVSTK_OK) {
        fprintf(stderr, "bvstkd: SMI service initialization failed: %s\n",
                bvstk_status_string(status));
        bvstk_smi_core_shutdown(&smi_core);
        bvstk_i2c_master_service_shutdown(&i2c);
        bvstk_i2c_master_hw_shutdown(&i2c_hardware);
        bvstk_i2c_policy_shutdown(&i2c_policy);
        bvstk_i2c_cache_shutdown(&i2c_cache);
        bvstk_i2c_devices_shutdown(&i2c_devices);
        return 1;
    }
    status = bvstk_spi_core_init(&spi, &default_spi_config, NULL, NULL);
    if (status != BVSTK_OK) {
        fprintf(stderr, "bvstkd: SPI initialization failed: %s\n",
                bvstk_status_string(status));
        bvstk_smi_service_shutdown(&smi);
        bvstk_smi_core_shutdown(&smi_core);
        bvstk_i2c_master_service_shutdown(&i2c);
        bvstk_i2c_master_hw_shutdown(&i2c_hardware);
        bvstk_i2c_policy_shutdown(&i2c_policy);
        bvstk_i2c_cache_shutdown(&i2c_cache);
        bvstk_i2c_devices_shutdown(&i2c_devices);
        return 1;
    }
    control.pl = &pl;
    control.i2c = &i2c;
    control.smi = &smi;
    control.spi = &spi;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("bvstkd: socket");
        return 1;
    }
    {
        int enabled = 1;
        (void)setsockopt(server_fd,
                         SOL_SOCKET,
                         SO_REUSEADDR,
                         &enabled,
                         sizeof(enabled));
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(BVSTKD_PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(server_fd, 2) != 0) {
        perror("bvstkd: bind/listen");
        close(server_fd);
        return 1;
    }

    printf("bvstkd: DCP2 listening on %u\n", BVSTKD_PORT);
    for (;;) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            continue;
        }
        (void)serve_client(client_fd, &control);
        close(client_fd);
    }
}
