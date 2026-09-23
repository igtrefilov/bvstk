#include "apps/freertos/console/i2c_shell.h"

#include <string.h>
#include <strings.h>

#include "apps/freertos/config/config_store.h"
#include "apps/freertos/console/bus_shell_common.h"
#include "apps/freertos/console/stream_shell.h"
#include "apps/freertos/runtime/bvstk_runtime.h"
#include "apps/freertos/services/dcp2/dcp2_stream_sim.h"

int i2c_complete_line(const char *line, size_t length, size_t cursor,
                      bvstk_i2c_completion_result_t *result)
{
    bvstk_i2c_completion_device_t entries[I2C_CFG_MAX_DEVICES];
    bvstk_i2c_master_service_t *service = bvstk_runtime_i2c_master_service();
    size_t count = bvstk_i2c_master_service_device_count(service), i;

    if (count > I2C_CFG_MAX_DEVICES) count = I2C_CFG_MAX_DEVICES;
    for (i = 0U; i < count; ++i) {
        bvstk_i2c_device_t device;
        if (bvstk_i2c_master_service_device_info(service, i, &device) != BVSTK_OK) break;
        memcpy(entries[i].name, device.name, sizeof(entries[i].name));
        entries[i].addr_7b = device.addr_7b;
    }
    return bvstk_i2c_complete_line_ex(line, length, cursor, entries, i,
                                      BVSTK_I2C_COMPLETION_EXPLICIT_SAVE, result);
}

static bool eq(const char *a, const char *b)
{
    return a != NULL && strcasecmp(a, b) == 0;
}

static void result(int fd, bvstk_status_t status)
{
    bus_shell_result(fd, "i2c", status);
}

static bvstk_status_t number(const char *text, unsigned long limit, unsigned long *value)
{
    return bus_shell_number(text, limit, value);
}

static bvstk_status_t select_device(const char *token, size_t *id,
                                    const i2c_device_config_t **config)
{
    bvstk_i2c_master_service_t *service = bvstk_runtime_i2c_master_service();
    bvstk_status_t status;
    unsigned long address;

    if (token[0] != '@' &&
        bvstk_i2c_master_service_find_by_name(service, token, id) == BVSTK_OK) {
        *config = config_store_find_i2c_device_by_name(token);
    } else {
        if (number(token[0] == '@' ? token + 1 : token, 0x7FU, &address) != BVSTK_OK) {
            return BVSTK_ERR_NOT_FOUND;
        }
        status = bvstk_i2c_master_service_find_by_addr(service, (uint8_t)address, id);
        if (status != BVSTK_OK) {
            return status;
        }
        *config = config_store_find_i2c_device_by_addr((uint8_t)address);
    }
    return *config != NULL ? BVSTK_OK : BVSTK_ERR_NOT_FOUND;
}

static bvstk_status_t apply_config(const i2c_device_config_t *config)
{
    if (!bvstk_runtime_i2c_apply_config(config) || !config_store_set_i2c_device(config)) {
        return BVSTK_ERR_INTERNAL;
    }
    return BVSTK_OK;
}

static void cmd_list(int fd, bvstk_i2c_master_service_t *service)
{
    size_t count = bvstk_i2c_master_service_device_count(service);

    bus_shell_writef(fd, "i2c: devices=%u\r\n", (unsigned)count);
    for (size_t i = 0U; i < count; ++i) {
        bvstk_i2c_device_t device;
        bvstk_status_t status = bvstk_i2c_master_service_device_info(service, i, &device);
        if (status != BVSTK_OK) {
            result(fd, status);
            return;
        }
        bus_shell_writef(fd, "  name=%s addr=0x%02X regs=%u max_value=%u\r\n",
                         device.name, (unsigned)device.addr_7b,
                         (unsigned)device.reg_count, (unsigned)device.max_value_code);
    }
}

static void cmd_info(int fd, size_t id, const i2c_device_config_t *config)
{
    bvstk_i2c_master_service_t *service = bvstk_runtime_i2c_master_service();
    bvstk_i2c_device_t device;
    bvstk_i2c_policy_entry_t policy;
    bvstk_status_t status = bvstk_i2c_master_service_device_info(service, id, &device);

    if (status == BVSTK_OK) {
        status = bvstk_i2c_master_service_get_policy(service, id, &policy);
    }
    if (status != BVSTK_OK) {
        result(fd, status);
        return;
    }
    bus_shell_writef(fd, "name=%s addr=0x%02X regs=%u max_value=%u policy=%s\r\n",
                     device.name, (unsigned)device.addr_7b,
                     (unsigned)device.reg_count, (unsigned)device.max_value_code,
                     policy.mode == I2C_POLICY_BLACKLIST ? "blacklist" : "whitelist");
    bus_shell_writef(fd, "file=%s whitelist=%u blacklist=%u settings=%u\r\n",
                     config->file_name, (unsigned)policy.whitelist_len,
                     (unsigned)policy.blacklist_len, (unsigned)config->settings_len);
}

static void cmd_read(int fd, size_t id, const i2c_device_config_t *config, const char *text)
{
    unsigned long reg;
    uint8_t value;
    bvstk_status_t status = number(text, config->reg_count - 1U, &reg);

    if (status == BVSTK_OK) {
        status = bvstk_i2c_master_service_read_from(bvstk_runtime_i2c_master_service(),
            id, (uint8_t)reg, &value, BVSTK_EVENT_SOURCE_CONSOLE, 100U);
    }
    if (status == BVSTK_OK) {
        bus_shell_read_result(fd, (uint8_t)reg, value, 2);
    } else {
        result(fd, status);
    }
}

static void cmd_write(int fd, size_t id, const i2c_device_config_t *config,
                      const char *reg_text, const char *value_text)
{
    unsigned long reg, value;
    bvstk_status_t status = number(reg_text, config->reg_count - 1U, &reg);

    if (status == BVSTK_OK) {
        status = number(value_text, config->max_value_code, &value);
    }
    if (status == BVSTK_OK) {
        status = bvstk_i2c_master_service_write(bvstk_runtime_i2c_master_service(),
                                               id, (uint8_t)reg, (uint8_t)value,
                                               BVSTK_EVENT_SOURCE_CONSOLE, 100U);
        if (status == BVSTK_OK && !bvstk_runtime_i2c_sync_device(id)) {
            status = BVSTK_ERR_INTERNAL;
        }
    }
    result(fd, status);
}

static void rules_show(int fd, const char *name, const i2c_rule_entry_t *rules, size_t count)
{
    bus_shell_rules_header(fd, name, count);
    for (size_t i = 0U; i < count; ++i) {
        bus_shell_rule(fd, rules[i].reg, rules[i].val, NULL);
    }
}

static void policy_show(int fd, size_t id, bus_shell_policy_view_t view)
{
    bvstk_i2c_policy_entry_t policy;
    bvstk_status_t status = bvstk_i2c_master_service_get_policy(
        bvstk_runtime_i2c_master_service(), id, &policy);

    if (status != BVSTK_OK) {
        result(fd, status);
        return;
    }
    if (view == BUS_SHELL_POLICY_MODE || view == BUS_SHELL_POLICY_RULES) {
        bus_shell_policy_mode(fd, policy.mode == I2C_POLICY_BLACKLIST);
    }
    if (view == BUS_SHELL_POLICY_RULES || view == BUS_SHELL_POLICY_WHITELIST) {
        rules_show(fd, "whitelist", policy.whitelist, policy.whitelist_len);
    }
    if (view == BUS_SHELL_POLICY_RULES || view == BUS_SHELL_POLICY_BLACKLIST) {
        rules_show(fd, "blacklist", policy.blacklist, policy.blacklist_len);
    }
}

static bvstk_status_t edit_rules(i2c_device_config_t *config, int argc, char **argv)
{
    i2c_rule_entry_t *rules;
    size_t *count, index;
    unsigned long reg, value;
    bvstk_status_t status;

    if (argc < 2) {
        return BVSTK_ERR_MALFORMED;
    }
    if (eq(argv[0], "whitelist")) {
        rules = config->whitelist;
        count = &config->whitelist_len;
    } else if (eq(argv[0], "blacklist")) {
        rules = config->blacklist;
        count = &config->blacklist_len;
    } else {
        return BVSTK_ERR_MALFORMED;
    }
    if (argc == 2 && eq(argv[1], "clear")) {
        *count = 0U;
        return BVSTK_OK;
    }
    if (argc != 4 || (!eq(argv[1], "add") && !eq(argv[1], "del") && !eq(argv[1], "delete"))) {
        return BVSTK_ERR_MALFORMED;
    }
    status = number(argv[2], config->reg_count - 1U, &reg);
    if (status == BVSTK_OK) {
        status = number(argv[3], config->max_value_code, &value);
    }
    if (status != BVSTK_OK) {
        return status;
    }
    for (index = 0U; index < *count; ++index) {
        if (rules[index].reg == reg && rules[index].val == value) {
            break;
        }
    }
    if (eq(argv[1], "add")) {
        if (index < *count) {
            return BVSTK_OK;
        }
        if (*count == I2C_CFG_RULES_MAX) {
            return BVSTK_ERR_RANGE;
        }
        rules[index].reg = (uint8_t)reg;
        rules[index].val = (uint8_t)value;
        ++*count;
    } else {
        if (index == *count) {
            return BVSTK_ERR_NOT_FOUND;
        }
        memmove(rules + index, rules + index + 1U, (*count - index - 1U) * sizeof(*rules));
        --*count;
    }
    return BVSTK_OK;
}

static void cmd_policy(int fd, size_t id, const i2c_device_config_t *config,
                        int argc, char **argv)
{
    bus_shell_policy_view_t view;
    bvstk_status_t status;
    i2c_device_config_t next;

    if (bus_shell_policy_view(argc, argv, &view)) {
        policy_show(fd, id, view);
        return;
    }
    next = *config;
    if (argc == 2 && eq(argv[0], "set")) {
        if (eq(argv[1], "whitelist")) {
            next.policy = I2C_POLICY_WHITELIST;
        } else if (eq(argv[1], "blacklist")) {
            next.policy = I2C_POLICY_BLACKLIST;
        } else {
            result(fd, BVSTK_ERR_MALFORMED);
            return;
        }
        status = apply_config(&next);
    } else {
        status = edit_rules(&next, argc, argv);
        if (status == BVSTK_OK) {
            status = apply_config(&next);
        }
    }
    if (status == BVSTK_OK) {
        bus_shell_changed(fd);
    } else {
        result(fd, status);
    }
}

static void cmd_save(int fd, size_t id, const char *name)
{
    const i2c_device_config_t *config;
    i2c_device_config_t snapshot;

    if (!bvstk_runtime_i2c_sync_device(id)) {
        result(fd, BVSTK_ERR_INTERNAL);
        return;
    }
    config = config_store_find_i2c_device_by_name(name);
    if (config == NULL) {
        result(fd, BVSTK_ERR_NOT_FOUND);
        return;
    }
    snapshot = *config;
    if (!config_store_save_i2c_device(&snapshot)) {
        result(fd, BVSTK_ERR_IO);
        return;
    }
    bus_shell_saved(fd, "i2c", snapshot.file_name);
}

/* Keep the large v2 configuration copy out of the dispatcher frame: save
 * already needs a snapshot and runtime cache synchronization on the stack. */
static void cmd_address(int fd, const i2c_device_config_t *config, const char *text)
{
    unsigned long address;
    i2c_device_config_t next = *config;
    bvstk_status_t status = number(text, 0x7FU, &address);
    if (status == BVSTK_OK) {
        next.addr_7b = (uint8_t)address;
        status = apply_config(&next);
    }
    if (status == BVSTK_OK) bus_shell_changed(fd);
    else result(fd, status);
}

bool i2c_handle(char *token, char **save, int fd)
{
    char *argv[8];
    int argc = 0;
    size_t id = 0U;
    const i2c_device_config_t *config = NULL;
    bvstk_i2c_master_service_t *service;
    bvstk_status_t status;

    if (!eq(token, "i2c")) {
        return false;
    }
    token = strtok_r(NULL, " \t", save);
    if (eq(token, "stream")) {
        (void)dcp2_stream_shell_handle(DCP2_STREAM_SERVICE_I2C, save, fd);
        return true;
    }
    while (token != NULL) {
        if (argc == (int)(sizeof(argv) / sizeof(argv[0]))) {
            result(fd, BVSTK_ERR_RANGE);
            return true;
        }
        argv[argc++] = token;
        token = strtok_r(NULL, " \t", save);
    }
    if (argc == 0 || (argc == 1 && (eq(argv[0], "-h") || eq(argv[0], "--help")))) {
        i2c_help(fd);
        return true;
    }
    service = bvstk_runtime_i2c_master_service();
    if (service == NULL) {
        result(fd, BVSTK_ERR_NOT_READY);
        return true;
    }
    if (argc == 1 && eq(argv[0], "list")) {
        cmd_list(fd, service);
        return true;
    }
    status = select_device(argv[0], &id, &config);
    if (status != BVSTK_OK) {
        result(fd, status);
        return true;
    }
    if (argc == 1 || (argc == 2 && eq(argv[1], "info"))) {
        cmd_info(fd, id, config);
    } else if (argc == 3 && (eq(argv[1], "r") || eq(argv[1], "read"))) {
        cmd_read(fd, id, config, argv[2]);
    } else if (argc == 4 && (eq(argv[1], "w") || eq(argv[1], "write"))) {
        cmd_write(fd, id, config, argv[2], argv[3]);
    } else if (argc == 3 && (eq(argv[1], "addr") || eq(argv[1], "address"))) {
        cmd_address(fd, config, argv[2]);
    } else if (argc == 2 && eq(argv[1], "save")) {
        cmd_save(fd, id, config->name);
    } else if (eq(argv[1], "policy")) {
        cmd_policy(fd, id, config, argc - 2, argv + 2);
    } else {
        result(fd, BVSTK_ERR_MALFORMED);
    }
    return true;
}

void i2c_help(int fd)
{
    write_str(fd,
        "i2c list | -h | --help\r\n"
        "i2c <name|@addr|addr> [info]\r\n"
        "i2c <sel> r <reg> | w <reg> <value>\r\n"
        "i2c <sel> addr <addr_7b>\r\n"
        "i2c <sel> policy [show]\r\n"
        "i2c <sel> policy show <rules|whitelist|blacklist>\r\n"
        "i2c <sel> policy <whitelist|blacklist>  (show list)\r\n"
        "i2c <sel> policy set <whitelist|blacklist>\r\n"
        "i2c <sel> policy <whitelist|blacklist> add|del <reg> <value>\r\n"
        "i2c <sel> policy <whitelist|blacklist> clear\r\n"
        "i2c <sel> save\r\n"
        "i2c stream fake enable [period_ms] [counter|ramp|toggle] [words]\r\n"
        "i2c stream fake disable|status\r\n"
        "Changes apply in RAM; save persists flash:/config/i2c/<device>.json.\r\n"
        "policy / policy show: mode only; policy show rules: mode and both lists.\r\n");
}
