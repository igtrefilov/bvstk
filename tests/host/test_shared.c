#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/boards/ax7020/bvstk_pl_regions.h"
#include "drivers/pl/i2c/bvstk_i2c_master.h"
#include "drivers/pl/i2c/bvstk_i2c_slave.h"
#include "drivers/pl/sd/bvstk_sd_controller.h"
#include "drivers/pl/smi/bvstk_smi_core.h"
#include "drivers/pl/spi/bvstk_spi_core.h"
#include "hardware/pl/i2c/bvstk_i2c_regs.h"
#include "protocols/dcp2/bvstk_dcp2_codec.h"
#include "protocols/dcp2/bvstk_dcp2_control.h"
#include "shared/base/bvstk_parse.h"
#include "shared/base/bvstk_status.h"
#include "shared/config/bvstk_config_model.h"
#include "shared/events/bvstk_event.h"
#include "shared/interfaces/bvstk_mmio.h"
#include "shared/pl/access/bvstk_pl_service.h"
#include "services/i2c/bvstk_i2c_master_service.h"
#include "services/i2c/bvstk_i2c_slave_service.h"
#include "services/smi/bvstk_smi_service.h"
#include "services/control/bvstk_control_api.h"

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n",                \
                    __FILE__, __LINE__, #condition);                         \
            return 1;                                                        \
        }                                                                    \
    } while (0)

uint64_t bvstk_platform_now_ms(void)
{
    return UINT64_C(1000);
}

void bvstk_platform_sleep_ms(uint32_t milliseconds)
{
    (void)milliseconds;
}

typedef struct {
    uint8_t i2c_value;
    uint16_t smi_value;
    unsigned reads;
    unsigned writes;
} fake_bus_t;

static bvstk_status_t fake_i2c_read(void *context,
                                    uint8_t addr_7b,
                                    uint8_t reg,
                                    uint8_t *value,
                                    uint32_t timeout_ms)
{
    fake_bus_t *bus = (fake_bus_t *)context;
    (void)addr_7b;
    (void)reg;
    (void)timeout_ms;
    bus->reads++;
    *value = bus->i2c_value;
    return BVSTK_OK;
}

static bvstk_status_t fake_i2c_write(void *context,
                                     uint8_t addr_7b,
                                     uint8_t reg,
                                     uint8_t value,
                                     uint32_t timeout_ms)
{
    fake_bus_t *bus = (fake_bus_t *)context;
    (void)addr_7b;
    (void)reg;
    (void)timeout_ms;
    bus->writes++;
    bus->i2c_value = value;
    return BVSTK_OK;
}

static bvstk_status_t fake_smi_read(void *context,
                                    uint8_t phy_addr,
                                    uint8_t reg,
                                    uint16_t *value,
                                    uint32_t timeout_ms)
{
    fake_bus_t *bus = (fake_bus_t *)context;
    (void)phy_addr;
    (void)reg;
    (void)timeout_ms;
    bus->reads++;
    *value = bus->smi_value;
    return BVSTK_OK;
}

static bvstk_status_t fake_smi_write(void *context,
                                     uint8_t phy_addr,
                                     uint8_t reg,
                                     uint16_t value,
                                     bvstk_event_source_t source,
                                     uint32_t timeout_ms)
{
    fake_bus_t *bus = (fake_bus_t *)context;
    (void)phy_addr;
    (void)reg;
    (void)source;
    (void)timeout_ms;
    bus->writes++;
    bus->smi_value = value;
    return BVSTK_OK;
}

typedef struct {
    unsigned count;
    uint16_t types[8];
    bvstk_event_t last;
} event_log_t;

static void record_event(void *context, const bvstk_event_t *event)
{
    event_log_t *log = (event_log_t *)context;
    if (log->count < sizeof(log->types) / sizeof(log->types[0])) {
        log->types[log->count] = event->type;
    }
    log->count++;
    log->last = *event;
}

int bvstk_mmio_region_open(bvstk_mmio_region_t *region,
                           uintptr_t physical_base,
                           size_t size)
{
    if (region == NULL || size == 0U) {
        errno = EINVAL;
        return -1;
    }
    region->mapped_base = calloc(1U, size);
    if (region->mapped_base == NULL) {
        errno = ENOMEM;
        return -1;
    }
    region->physical_base = physical_base;
    region->size = size;
    return 0;
}

void bvstk_mmio_region_close(bvstk_mmio_region_t *region)
{
    if (region == NULL) {
        return;
    }
    free((void *)region->mapped_base);
    memset(region, 0, sizeof(*region));
}

static int access_valid(const bvstk_mmio_region_t *region, size_t offset)
{
    return region != NULL && region->mapped_base != NULL &&
           (offset & 3U) == 0U && region->size >= sizeof(uint32_t) &&
           offset <= region->size - sizeof(uint32_t);
}

int bvstk_mmio_read32(const bvstk_mmio_region_t *region,
                      size_t offset,
                      uint32_t *value)
{
    if (value == NULL || !access_valid(region, offset)) {
        errno = EINVAL;
        return -1;
    }
    memcpy(value, (const void *)(region->mapped_base + offset), sizeof(*value));
    return 0;
}

int bvstk_mmio_write32(const bvstk_mmio_region_t *region,
                       size_t offset,
                       uint32_t value)
{
    if (!access_valid(region, offset)) {
        errno = EINVAL;
        return -1;
    }
    memcpy((void *)(region->mapped_base + offset), &value, sizeof(value));
    return 0;
}

int main(void)
{
    const bvstk_pl_region_desc_t *spi;
    bvstk_pl_service_t service;
    bvstk_status_t status;
    uint32_t value = 0U;
    bool ok = false;
    fake_bus_t bus = {.i2c_value = 0x55U, .smi_value = 0x1234U};
    event_log_t events = {0};
    bvstk_i2c_devices_t i2c_devices;
    bvstk_i2c_cache_t i2c_cache;
    bvstk_i2c_policy_t i2c_policy;
    bvstk_i2c_master_service_t i2c_service;
    bvstk_i2c_master_hw_t i2c_master_hw;
    bvstk_i2c_slave_hw_t i2c_slave_hw;
    bvstk_i2c_slave_service_t i2c_slave_service;
    bvstk_sd_controller_t sd_controller;
    bvstk_sd_controller_status_t sd_status;
    bvstk_smi_service_t smi_service;
    bvstk_i2c_master_io_t i2c_ops = {
        .context = &bus,
        .read_reg = fake_i2c_read,
        .write_reg = fake_i2c_write
    };
    bvstk_smi_bus_ops_t smi_ops = {
        .context = &bus,
        .read = fake_smi_read,
        .write = fake_smi_write
    };
    bvstk_event_sink_t event_sink = {
        .context = &events,
        .publish = record_event
    };
    i2c_device_config_t i2c_config = {
        .name = "sensor",
        .file_name = "sensor.json",
        .addr_7b = 0x50,
        .reg_count = 16,
        .max_value_code = 10,
        .policy = I2C_POLICY_BLACKLIST,
        .blacklist = {{.reg = 3, .val = 7}},
        .blacklist_len = 1
    };
    smi_phy_config_t smi_config = {
        .name = "phy",
        .file_name = "phy.json",
        .phy_addr = 1,
        .reg_count = 32,
        .policy = SMI_POLICY_BLACKLIST,
        .write_deny_regs = {4},
        .write_deny_regs_len = 1
    };
    uint8_t value8 = 0U;
    uint16_t value16 = 0U;
    uint8_t frame[32] = {0};
    uint8_t response[32] = {0};
    bvstk_dcp2_request_t request;
    bvstk_control_api_t control = {0};
    size_t response_size = 0U;

    CHECK(strcmp(bvstk_status_string(BVSTK_ERR_TIMEOUT), "timeout") == 0);
    CHECK(parse_num("42", &ok) == 42UL && ok);
    CHECK(parse_num("0x2a", &ok) == 42UL && ok);
    (void)parse_num("42x", &ok);
    CHECK(!ok);

    CHECK(bvstk_pl_region_count() == (size_t)BVSTK_PL_REGION_COUNT);
    spi = bvstk_pl_region_find("spi-master");
    CHECK(spi != NULL && spi->id == BVSTK_PL_SPI_MASTER);

    bvstk_pl_service_init(&service);
    status = bvstk_pl_service_write32(&service, BVSTK_PL_SPI_MASTER,
                                      0U, UINT32_C(0x12345678));
    CHECK(status == BVSTK_OK);
    status = bvstk_pl_service_read32(&service, BVSTK_PL_SPI_MASTER, 0U, &value);
    CHECK(status == BVSTK_OK && value == UINT32_C(0x12345678));
    status = bvstk_pl_service_read32(&service, BVSTK_PL_SPI_MASTER,
                                     spi->size, &value);
    CHECK(status == BVSTK_ERR_RANGE);
    bvstk_pl_service_shutdown(&service);

    CHECK(bvstk_sd_controller_init(&sd_controller, NULL, NULL, NULL) == BVSTK_OK);
    CHECK(bvstk_sd_controller_get_status(&sd_controller, &sd_status) == BVSTK_OK);
    CHECK(!sd_status.initialized && !sd_status.error);
    bvstk_sd_controller_shutdown(&sd_controller);

    CHECK(bvstk_i2c_devices_init_from_config(&i2c_devices,
                                             &i2c_config,
                                             1U) == BVSTK_OK);
    CHECK(bvstk_i2c_cache_init(&i2c_cache, 1U) == BVSTK_OK);
    CHECK(bvstk_i2c_policy_init(&i2c_policy, &i2c_config, 1U) == BVSTK_OK);
    CHECK(bvstk_i2c_master_service_init(&i2c_service,
                                        NULL,
                                        &i2c_ops,
                                        &i2c_devices,
                                        &i2c_cache,
                                        &i2c_policy,
                                        &event_sink) == BVSTK_OK);
    CHECK(bvstk_i2c_master_service_read(&i2c_service,
                                        0U,
                                        1U,
                                        &value8,
                                        10U) == BVSTK_OK);
    CHECK(value8 == 0x55U && bus.reads == 1U);
    CHECK(bvstk_i2c_master_service_write(&i2c_service,
                                         0U,
                                         3U,
                                         7U,
                                         BVSTK_EVENT_SOURCE_DCP,
                                         10U) == BVSTK_ERR_DENIED);
    CHECK(bvstk_i2c_master_service_write(&i2c_service,
                                         0U,
                                         2U,
                                         5U,
                                         BVSTK_EVENT_SOURCE_DCP,
                                         10U) == BVSTK_OK);
    CHECK(bus.writes == 1U && events.last.source == BVSTK_EVENT_SOURCE_DCP);
    CHECK(events.count == 4U && events.types[1] == BVSTK_EVENT_REG_DENIED &&
          events.types[3] == BVSTK_EVENT_REG_COMMIT);

    CHECK(bvstk_i2c_master_hw_init(&i2c_master_hw, NULL, NULL) == BVSTK_OK);
    {
        uint32_t word0 = UINT32_C(0x0000BBAA);
        uint32_t word1 = UINT32_C(0x0000DDCC);
        uint8_t raw_read[2] = {0U, 0U};

        memcpy((void *)(i2c_master_hw.bram.mapped_base +
                        BVSTK_I2C_BRAM_MASTER_OFFSET + sizeof(uint32_t)),
               &word0,
               sizeof(word0));
        memcpy((void *)(i2c_master_hw.bram.mapped_base +
                        BVSTK_I2C_BRAM_MASTER_OFFSET + 2U * sizeof(uint32_t)),
               &word1,
               sizeof(word1));
        CHECK(bvstk_i2c_master_hw_transfer(&i2c_master_hw,
                                           0x50U,
                                           NULL,
                                           0U,
                                           raw_read,
                                           sizeof(raw_read),
                                           10U) == BVSTK_OK);
        CHECK(raw_read[0] == 0xAAU && raw_read[1] == 0xBBU);
    }
    bvstk_i2c_master_hw_shutdown(&i2c_master_hw);

    CHECK(bvstk_i2c_cache_write(&i2c_cache, 0U, 2U, 0xA5U) == BVSTK_OK);
    CHECK(bvstk_i2c_slave_service_init(&i2c_slave_service,
                                       &i2c_devices,
                                       &i2c_cache,
                                       &i2c_service,
                                       0U) == BVSTK_OK);
    {
        uint8_t pointer_frame[] = {2U};
        size_t read_window_size = 0U;
        CHECK(bvstk_i2c_slave_service_handle_frame(&i2c_slave_service,
                                                   pointer_frame,
                                                   sizeof(pointer_frame),
                                                   0U,
                                                   response,
                                                   sizeof(response),
                                                   &read_window_size,
                                                   10U) == BVSTK_OK);
        CHECK(read_window_size != 0U && response[0] == 0xA5U);
    }
    bvstk_i2c_slave_service_shutdown(&i2c_slave_service);

    CHECK(bvstk_i2c_slave_hw_init(&i2c_slave_hw) == BVSTK_OK);
    CHECK(bvstk_i2c_slave_hw_set_address(&i2c_slave_hw, 0x36U) == BVSTK_OK);
    {
        uint32_t word = 0U;
        uint32_t request_header = (3U << BVSTK_I2C_SLV_HEADER_BYTES_SHIFT) | 0x36U;
        uint32_t write_header = (2U << BVSTK_I2C_SLV_HEADER_BYTES_SHIFT) | 0x36U;
        uint8_t slave_frame[4] = {0U};
        uint8_t slave_response[] = {0x11U, 0x22U, 0x33U};
        size_t slave_frame_size = 0U;
        uint8_t slave_addr = 0U;
        bvstk_i2c_slave_irq_event_t slave_event;

        memcpy(&word,
               (const void *)(i2c_slave_hw.slave.mapped_base +
                              BVSTK_I2C_SLV_ADDR_LIST_1),
               sizeof(word));
        CHECK(word == (UINT32_C(1) << 22));
        memcpy((void *)(i2c_slave_hw.slave.mapped_base +
                        BVSTK_I2C_SLV_IRQ_OFFSET),
               &(uint32_t){BVSTK_I2C_SLV_IRQ_DATA_VALID |
                           BVSTK_I2C_SLV_IRQ_RD_REQUEST},
               sizeof(word));
        memcpy((void *)(i2c_slave_hw.slave.mapped_base +
                        BVSTK_I2C_SLV_REQ_OFFSET),
               &request_header,
               sizeof(request_header));
        CHECK(bvstk_i2c_slave_hw_capture_irq(&i2c_slave_hw,
                                             &slave_event) == BVSTK_OK);
        CHECK(slave_event.irq_flags ==
                  (BVSTK_I2C_SLV_IRQ_DATA_VALID |
                   BVSTK_I2C_SLV_IRQ_RD_REQUEST) &&
              slave_event.request_addr == 0x36U &&
              slave_event.request_size == 3U);
        memcpy(&word,
               (const void *)(i2c_slave_hw.slave.mapped_base +
                              BVSTK_I2C_SLV_IRQ_OFFSET),
               sizeof(word));
        CHECK(word == BVSTK_I2C_SLV_IRQ_RESET_BIT);

        memcpy((void *)(i2c_slave_hw.bram.mapped_base +
                        BVSTK_I2C_BRAM_SLAVE_WR_OFFSET),
               &write_header,
               sizeof(write_header));
        word = UINT32_C(0x0000BBAA);
        memcpy((void *)(i2c_slave_hw.bram.mapped_base +
                        BVSTK_I2C_BRAM_SLAVE_WR_OFFSET + sizeof(word)),
               &word,
               sizeof(word));
        CHECK(bvstk_i2c_slave_hw_read_frame(&i2c_slave_hw,
                                            slave_frame,
                                            sizeof(slave_frame),
                                            &slave_frame_size,
                                            &slave_addr) == BVSTK_OK);
        CHECK(slave_frame_size == 2U && slave_addr == 0x36U &&
              slave_frame[0] == 0xAAU && slave_frame[1] == 0xBBU);

        CHECK(bvstk_i2c_slave_hw_write_read_window(&i2c_slave_hw,
                                                   0x36U,
                                                   slave_response,
                                                   sizeof(slave_response)) == BVSTK_OK);
        memcpy(&word,
               (const void *)(i2c_slave_hw.bram.mapped_base +
                              BVSTK_I2C_BRAM_SLAVE_RD_OFFSET),
               sizeof(word));
        CHECK(word == (((uint32_t)sizeof(slave_response) <<
                        BVSTK_I2C_SLV_HEADER_BYTES_SHIFT) | 0x36U));
        memcpy(&word,
               (const void *)(i2c_slave_hw.bram.mapped_base +
                              BVSTK_I2C_BRAM_SLAVE_RD_OFFSET + sizeof(word)),
               sizeof(word));
        CHECK(word == UINT32_C(0x00332211));
        CHECK(bvstk_i2c_slave_hw_accept_read(&i2c_slave_hw) == BVSTK_OK);
        memcpy(&word,
               (const void *)(i2c_slave_hw.slave.mapped_base +
                              BVSTK_I2C_SLV_CSR_OFFSET),
               sizeof(word));
        CHECK(word == BVSTK_I2C_SLV_CSR_RD_VALID_BIT);
    }
    bvstk_i2c_slave_hw_shutdown(&i2c_slave_hw);
    CHECK(bvstk_smi_service_init(&smi_service,
                                 NULL,
                                 &smi_ops,
                                 &smi_config,
                                 1U,
                                 NULL) == BVSTK_OK);
    CHECK(bvstk_smi_service_read(&smi_service, 0U, 1U, &value16, 10U) == BVSTK_OK);
    CHECK(value16 == 0x1234U);
    CHECK(bvstk_smi_service_write(&smi_service,
                                  0U,
                                  4U,
                                  0x22U,
                                  BVSTK_EVENT_SOURCE_CONSOLE,
                                  10U) == BVSTK_ERR_DENIED);
    CHECK(bvstk_smi_service_write(&smi_service,
                                  0U,
                                  2U,
                                  0x4321U,
                                  BVSTK_EVENT_SOURCE_CONSOLE,
                                  10U) == BVSTK_OK);
    CHECK(bus.smi_value == 0x4321U);
    bvstk_smi_service_shutdown(&smi_service);

    frame[0] = 'D'; frame[1] = 'C'; frame[2] = 'P'; frame[3] = '2';
    bvstk_dcp2_write_be16(frame + 4, BVSTK_DCP2_VERSION);
    bvstk_dcp2_write_be16(frame + 6, 6U);
    frame[8] = BVSTK_DCP2_SERVICE_I2C;
    frame[9] = BVSTK_DCP2_OP_I2C_READ_REG;
    bvstk_dcp2_write_be16(frame + 10, 1U);
    frame[12] = 0x50U;
    frame[13] = 0x02U;
    CHECK(bvstk_dcp2_decode_request(frame, 14U, &request) == 0);
    CHECK(request.service == BVSTK_DCP2_SERVICE_I2C &&
          request.opcode == BVSTK_DCP2_OP_I2C_READ_REG &&
          request.sequence == 1U && request.body_size == 2U);
    bus.i2c_value = 0x55U;
    control.i2c = &i2c_service;
    CHECK(bvstk_dcp2_process_request(&control,
                                     &request,
                                     response,
                                     sizeof(response),
                                     &response_size) == 0);
    CHECK(response_size == 15U && response[14] == 0x55U);
    CHECK(bvstk_dcp2_encode_response(response,
                                     sizeof(response),
                                     &request,
                                     0U,
                                     (const uint8_t *)"\xAA",
                                     1U,
                                     &response_size) == 0);
    CHECK(response_size == 15U && response[9] == BVSTK_DCP2_OP_RESPONSE &&
          bvstk_dcp2_read_be16(response + 12) == 0U && response[14] == 0xAAU);
    bvstk_i2c_master_service_shutdown(&i2c_service);
    bvstk_i2c_policy_shutdown(&i2c_policy);
    bvstk_i2c_cache_shutdown(&i2c_cache);
    bvstk_i2c_devices_shutdown(&i2c_devices);

    puts("shared host tests passed");
    return 0;
}
