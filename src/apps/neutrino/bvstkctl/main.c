#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "hardware/boards/ax7020/bvstk_pl_regions.h"
#include "hardware/pl/spi/bvstk_spi_regs.h"
#include "drivers/pl/i2c/bvstk_i2c_master.h"
#include "services/control/bvstk_control_api.h"
#include "services/i2c/bvstk_i2c_master_service.h"
#include "shared/base/bvstk_status.h"
#include "shared/config/bvstk_config_model.h"
#include "shared/interfaces/bvstk_platform.h"
#include "shared/pl/access/bvstk_pl_service.h"

#define BVSTKCTL_VERSION "0.1.0"
#define BVSTKCTL_MAX_READ_WORDS 64U

static void usage(FILE *stream)
{
    fprintf(stream,
            "Usage:\n"
            "  bvstkctl version\n"
            "  bvstkctl pl list\n"
            "  bvstkctl pl probe\n"
            "  bvstkctl pl read <region> <offset> [word-count]\n"
            "  bvstkctl pl write <region> <offset> <value>\n"
            "  bvstkctl i2c list\n"
            "  bvstkctl i2c read <device> <register>\n"
            "  bvstkctl i2c write <device> <register> <value>\n"
            "\n"
            "Numbers accept decimal or 0x-prefixed hexadecimal notation.\n");
}

static int parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (text == NULL || value == NULL || *text == '\0') {
        return -1;
    }
    errno = 0;
    parsed = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
        return -1;
    }
    *value = (uint32_t)parsed;
    return 0;
}

static int command_version(void)
{
    printf("bvstkctl %s\n", BVSTKCTL_VERSION);
    printf("platform=%s\n", bvstk_platform_name());
    printf("pl_contract=%" PRIu32 "\n", BVSTK_PL_CONTRACT_VERSION);
    return 0;
}

static int command_pl_list(void)
{
    size_t i;

    for (i = 0; i < bvstk_pl_region_count(); ++i) {
        const bvstk_pl_region_desc_t *desc =
            bvstk_pl_region_get((bvstk_pl_region_id_t)i);
        printf("%-12s base=0x%08" PRIxPTR " size=0x%zx kind=%s\n",
               desc->name,
               desc->physical_base,
               desc->size,
               desc->kind == BVSTK_PL_REGION_BRAM ? "bram" : "control");
    }
    return 0;
}

static int print_service_error(const char *operation, bvstk_status_t status)
{
    fprintf(stderr, "bvstkctl: %s failed: %s", operation, bvstk_status_string(status));
    if (errno != 0) {
        fprintf(stderr, " (%s)", strerror(errno));
    }
    fputc('\n', stderr);
    return 1;
}

static int command_pl_read(const char *region_name,
                           const char *offset_text,
                           const char *count_text)
{
    const bvstk_pl_region_desc_t *desc = bvstk_pl_region_find(region_name);
    bvstk_pl_service_t service;
    uint32_t offset;
    uint32_t count = 1U;
    uint32_t i;

    if (desc == NULL) {
        fprintf(stderr, "bvstkctl: unknown PL region: %s\n", region_name);
        return 2;
    }
    if (parse_u32(offset_text, &offset) != 0 ||
        (count_text != NULL && parse_u32(count_text, &count) != 0) ||
        count == 0U || count > BVSTKCTL_MAX_READ_WORDS) {
        fprintf(stderr, "bvstkctl: invalid offset or word count\n");
        return 2;
    }
    if ((offset & 3U) != 0U ||
        (size_t)offset > desc->size ||
        (size_t)count * sizeof(uint32_t) > desc->size - (size_t)offset) {
        fprintf(stderr, "bvstkctl: requested range is outside %s\n", desc->name);
        return 2;
    }

    bvstk_pl_service_init(&service);
    for (i = 0; i < count; ++i) {
        uint32_t value;
        size_t current_offset = (size_t)offset + (size_t)i * sizeof(uint32_t);
        bvstk_status_t status = bvstk_pl_service_read32(&service,
                                                        desc->id,
                                                        current_offset,
                                                        &value);
        if (status != BVSTK_OK) {
            bvstk_pl_service_shutdown(&service);
            return print_service_error("PL read", status);
        }
        printf("%s+0x%04zx = 0x%08" PRIx32 "\n",
               desc->name,
               current_offset,
               value);
    }
    bvstk_pl_service_shutdown(&service);
    return 0;
}

static int command_pl_write(const char *region_name,
                            const char *offset_text,
                            const char *value_text)
{
    const bvstk_pl_region_desc_t *desc = bvstk_pl_region_find(region_name);
    bvstk_pl_service_t service;
    bvstk_status_t status;
    uint32_t offset;
    uint32_t value;

    if (desc == NULL) {
        fprintf(stderr, "bvstkctl: unknown PL region: %s\n", region_name);
        return 2;
    }
    if (parse_u32(offset_text, &offset) != 0 || parse_u32(value_text, &value) != 0) {
        fprintf(stderr, "bvstkctl: invalid offset or value\n");
        return 2;
    }
    if ((offset & 3U) != 0U ||
        desc->size < sizeof(uint32_t) ||
        (size_t)offset > desc->size - sizeof(uint32_t)) {
        fprintf(stderr, "bvstkctl: write offset is outside %s\n", desc->name);
        return 2;
    }

    bvstk_pl_service_init(&service);
    status = bvstk_pl_service_write32(&service, desc->id, offset, value);
    bvstk_pl_service_shutdown(&service);
    if (status != BVSTK_OK) {
        return print_service_error("PL write", status);
    }
    printf("%s+0x%04" PRIx32 " <- 0x%08" PRIx32 "\n",
           desc->name,
           offset,
           value);
    return 0;
}

static size_t probe_offset_for(const bvstk_pl_region_desc_t *desc)
{
    if (desc->id == BVSTK_PL_SPI_MASTER) {
        return BVSTK_SPI_SIGNATURE_OFFSET;
    }
    return 0U;
}

static int command_pl_probe(void)
{
    bvstk_pl_service_t service;
    size_t i;
    int result = 0;

    bvstk_pl_service_init(&service);
    for (i = 0; i < bvstk_pl_region_count(); ++i) {
        const bvstk_pl_region_desc_t *desc =
            bvstk_pl_region_get((bvstk_pl_region_id_t)i);
        uint32_t value;
        size_t offset;
        bvstk_status_t status;

        if (desc->kind != BVSTK_PL_REGION_CONTROL) {
            continue;
        }
        offset = probe_offset_for(desc);
        status = bvstk_pl_service_read32(&service, desc->id, offset, &value);
        if (status == BVSTK_OK) {
            printf("%-12s offset=0x%02zx value=0x%08" PRIx32 "\n",
                   desc->name,
                   offset,
                   value);
        } else {
            fprintf(stderr, "%-12s error=%s\n", desc->name, bvstk_status_string(status));
            result = 1;
        }
    }
    bvstk_pl_service_shutdown(&service);
    return result;
}

static int i2c_runtime_open(bvstk_i2c_master_hw_t *hardware,
                            bvstk_i2c_devices_t *devices,
                            bvstk_i2c_cache_t *cache,
                            bvstk_i2c_policy_t *policy,
                            bvstk_i2c_master_service_t *service)
{
    static const i2c_device_config_t default_device = {
        .name = "default",
        .file_name = "default.json",
        .addr_7b = 0x50,
        .reg_count = 256,
        .max_value_code = 64,
        .policy = I2C_POLICY_BLACKLIST
    };
    bvstk_status_t status;

    status = bvstk_i2c_devices_init_from_config(devices, &default_device, 1U);
    if (status == BVSTK_OK) {
        status = bvstk_i2c_cache_init(cache, 1U);
    }
    if (status == BVSTK_OK) {
        status = bvstk_i2c_policy_init(policy, &default_device, 1U);
    }
    if (status == BVSTK_OK) {
        status = bvstk_i2c_master_hw_init(hardware, NULL, NULL);
    }
    if (status != BVSTK_OK) {
        fprintf(stderr, "bvstkctl: I2C hardware init failed: %s\n",
                bvstk_status_string(status));
        return 1;
    }
    status = bvstk_i2c_master_service_init(service,
                                           hardware,
                                           NULL,
                                           devices,
                                           cache,
                                           policy,
                                           NULL);
    if (status != BVSTK_OK) {
        bvstk_i2c_master_hw_shutdown(hardware);
        bvstk_i2c_policy_shutdown(policy);
        bvstk_i2c_cache_shutdown(cache);
        bvstk_i2c_devices_shutdown(devices);
        fprintf(stderr, "bvstkctl: I2C service init failed: %s\n",
                bvstk_status_string(status));
        return 1;
    }
    return 0;
}

static void i2c_runtime_close(bvstk_i2c_master_hw_t *hardware,
                              bvstk_i2c_devices_t *devices,
                              bvstk_i2c_cache_t *cache,
                              bvstk_i2c_policy_t *policy,
                              bvstk_i2c_master_service_t *service)
{
    bvstk_i2c_master_service_shutdown(service);
    bvstk_i2c_master_hw_shutdown(hardware);
    bvstk_i2c_policy_shutdown(policy);
    bvstk_i2c_cache_shutdown(cache);
    bvstk_i2c_devices_shutdown(devices);
}

static int i2c_device_id(const bvstk_i2c_master_service_t *service,
                         const char *text,
                         size_t *device_id)
{
    uint32_t address;

    if (parse_u32(text, &address) == 0 && address <= 0x7FU) {
        return bvstk_i2c_master_service_find_by_addr(service,
                                                     (uint8_t)address,
                                                     device_id) == BVSTK_OK
                   ? 0
                   : -1;
    }
    return bvstk_i2c_master_service_find_by_name(service, text, device_id) == BVSTK_OK
               ? 0
               : -1;
}

static int command_i2c_list(void)
{
    bvstk_i2c_master_hw_t hardware;
    bvstk_i2c_devices_t devices;
    bvstk_i2c_cache_t cache;
    bvstk_i2c_policy_t policy;
    bvstk_i2c_master_service_t service;
    size_t i;

    if (i2c_runtime_open(&hardware, &devices, &cache, &policy, &service) != 0) {
        return 1;
    }
    for (i = 0; i < bvstk_i2c_master_service_device_count(&service); ++i) {
        bvstk_i2c_device_t info;
        if (bvstk_i2c_master_service_device_info(&service, i, &info) == BVSTK_OK) {
            printf("%s addr=0x%02" PRIx8 " regs=%" PRIu16 " max=%" PRIu8 "\n",
                   info.name,
                   info.addr_7b,
                   info.reg_count,
                   info.max_value_code);
        }
    }
    i2c_runtime_close(&hardware, &devices, &cache, &policy, &service);
    return 0;
}

static int command_i2c_read(const char *device_text, const char *reg_text)
{
    bvstk_i2c_master_hw_t hardware;
    bvstk_i2c_devices_t devices;
    bvstk_i2c_cache_t cache;
    bvstk_i2c_policy_t policy;
    bvstk_i2c_master_service_t service;
    uint32_t reg;
    size_t device_id;
    uint8_t value = 0;
    bvstk_status_t status;

    if (parse_u32(reg_text, &reg) != 0 || reg > 0xFFU ||
        i2c_runtime_open(&hardware, &devices, &cache, &policy, &service) != 0) {
        fprintf(stderr, "bvstkctl: invalid I2C device/register\n");
        return 2;
    }
    if (i2c_device_id(&service, device_text, &device_id) != 0) {
        i2c_runtime_close(&hardware, &devices, &cache, &policy, &service);
        fprintf(stderr, "bvstkctl: unknown I2C device: %s\n", device_text);
        return 2;
    }
    status = bvstk_i2c_master_service_read(&service,
                                           device_id,
                                           (uint8_t)reg,
                                           &value,
                                           100);
    i2c_runtime_close(&hardware, &devices, &cache, &policy, &service);
    if (status != BVSTK_OK) {
        return print_service_error("I2C read", status);
    }
    printf("i2c[%s]+0x%02" PRIx32 " = 0x%02" PRIx8 "\n",
           device_text, reg, value);
    return 0;
}

static int command_i2c_write(const char *device_text,
                             const char *reg_text,
                             const char *value_text)
{
    bvstk_i2c_master_hw_t hardware;
    bvstk_i2c_devices_t devices;
    bvstk_i2c_cache_t cache;
    bvstk_i2c_policy_t policy;
    bvstk_i2c_master_service_t service;
    uint32_t reg;
    uint32_t value;
    size_t device_id;
    bvstk_status_t status;

    if (parse_u32(reg_text, &reg) != 0 || reg > 0xFFU ||
        parse_u32(value_text, &value) != 0 || value > 0xFFU ||
        i2c_runtime_open(&hardware, &devices, &cache, &policy, &service) != 0) {
        fprintf(stderr, "bvstkctl: invalid I2C device/register/value\n");
        return 2;
    }
    if (i2c_device_id(&service, device_text, &device_id) != 0) {
        i2c_runtime_close(&hardware, &devices, &cache, &policy, &service);
        fprintf(stderr, "bvstkctl: unknown I2C device: %s\n", device_text);
        return 2;
    }
    status = bvstk_i2c_master_service_write(&service,
                                            device_id,
                                            (uint8_t)reg,
                                            (uint8_t)value,
                                            BVSTK_EVENT_SOURCE_CONSOLE,
                                            100);
    i2c_runtime_close(&hardware, &devices, &cache, &policy, &service);
    if (status != BVSTK_OK) {
        return print_service_error("I2C write", status);
    }
    printf("i2c[%s]+0x%02" PRIx32 " <- 0x%02" PRIx32 "\n",
           device_text, reg, value);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "version") == 0) {
        return command_version();
    }
    if (argc >= 3 && strcmp(argv[1], "pl") == 0) {
        if (argc == 3 && strcmp(argv[2], "list") == 0) {
            return command_pl_list();
        }
        if (argc == 3 && strcmp(argv[2], "probe") == 0) {
            return command_pl_probe();
        }
        if ((argc == 5 || argc == 6) && strcmp(argv[2], "read") == 0) {
            return command_pl_read(argv[3], argv[4], argc == 6 ? argv[5] : NULL);
        }
        if (argc == 6 && strcmp(argv[2], "write") == 0) {
            return command_pl_write(argv[3], argv[4], argv[5]);
        }
    }
    if (argc == 3 && strcmp(argv[1], "i2c") == 0 &&
        strcmp(argv[2], "list") == 0) {
        return command_i2c_list();
    }
    if (argc == 5 && strcmp(argv[1], "i2c") == 0 &&
        strcmp(argv[2], "read") == 0) {
        return command_i2c_read(argv[3], argv[4]);
    }
    if (argc == 6 && strcmp(argv[1], "i2c") == 0 &&
        strcmp(argv[2], "write") == 0) {
        return command_i2c_write(argv[3], argv[4], argv[5]);
    }

    usage(stderr);
    return 2;
}
