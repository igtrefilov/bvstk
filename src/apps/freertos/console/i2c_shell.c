#include "apps/freertos/console/i2c_shell.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "apps/freertos/config/config_store.h"
#include "apps/freertos/runtime/bvstk_runtime.h"

static void i2c_writef(int fd, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) {
        return;
    }
    if (n >= (int)sizeof(buf)) {
        n = (int)sizeof(buf) - 1;
    }
    (void)console_stream_write(fd, buf, (size_t)n);
}

static bool rule_contains(const i2c_rule_entry_t *rules,
                          size_t length,
                          uint8_t reg,
                          uint8_t value)
{
    for (size_t i = 0U; i < length; ++i) {
        if (rules[i].reg == reg && rules[i].val == value) {
            return true;
        }
    }
    return false;
}

static void rule_add(i2c_rule_entry_t *rules,
                     size_t *length,
                     uint8_t reg,
                     uint8_t value)
{
    if (!rule_contains(rules, *length, reg, value) &&
        *length < I2C_CFG_RULES_MAX) {
        rules[*length].reg = reg;
        rules[*length].val = value;
        (*length)++;
    }
}

static void rule_remove(i2c_rule_entry_t *rules,
                        size_t *length,
                        uint8_t reg,
                        uint8_t value)
{
    size_t write_index = 0U;

    for (size_t i = 0U; i < *length; ++i) {
        if (rules[i].reg == reg && rules[i].val == value) {
            continue;
        }
        rules[write_index++] = rules[i];
    }
    *length = write_index;
}

static bool parse_selector(const char *token,
                           size_t *out_index,
                           const i2c_device_config_t **out_config)
{
    bvstk_i2c_master_service_t *service =
        bvstk_runtime_i2c_master_service();
    size_t index = 0U;
    const i2c_device_config_t *config = NULL;

    if (out_index != NULL) {
        *out_index = 0U;
    }
    if (out_config != NULL) {
        *out_config = NULL;
    }
    if (service == NULL || token == NULL || token[0] == '\0') {
        return false;
    }
    if (token[0] == '@') {
        bool ok = false;
        unsigned long address = parse_num(token + 1, &ok);
        if (!ok || address > 0x7FU ||
            bvstk_i2c_master_service_find_by_addr(service,
                                                  (uint8_t)address,
                                                  &index) != BVSTK_OK) {
            return false;
        }
        config = config_store_find_i2c_device_by_addr((uint8_t)address);
    } else if (bvstk_i2c_master_service_find_by_name(service,
                                                     token,
                                                     &index) == BVSTK_OK) {
        config = config_store_find_i2c_device_by_name(token);
    } else {
        bool ok = false;
        unsigned long address = parse_num(token, &ok);
        if (!ok || address > 0x7FU ||
            bvstk_i2c_master_service_find_by_addr(service,
                                                  (uint8_t)address,
                                                  &index) != BVSTK_OK) {
            return false;
        }
        config = config_store_find_i2c_device_by_addr((uint8_t)address);
    }
    if (config == NULL) {
        return false;
    }
    if (out_index != NULL) {
        *out_index = index;
    }
    if (out_config != NULL) {
        *out_config = config;
    }
    return true;
}

static bool persist_config(int fd, const i2c_device_config_t *config)
{
    if (config == NULL ||
        !bvstk_runtime_i2c_apply_config(config) ||
        !config_store_set_i2c_device(config)) {
        write_str(fd, "ERR (failed to apply configuration)\r\n");
        return false;
    }
    if (!config_store_save_i2c_device(config)) {
        write_str(fd, "WARN: failed to save to flash:/config/i2c/<device>.json\r\n");
    }
    return true;
}

static void cmd_list(int fd)
{
    bvstk_i2c_master_service_t *service =
        bvstk_runtime_i2c_master_service();
    size_t count;

    if (service == NULL) {
        write_str(fd, "ERR (I2C not ready)\r\n");
        return;
    }
    count = bvstk_i2c_master_service_device_count(service);
    i2c_writef(fd, "I2C devices: %u\r\n", (unsigned)count);
    for (size_t i = 0U; i < count; ++i) {
        bvstk_i2c_device_t device;
        if (bvstk_i2c_master_service_device_info(service, i, &device) != BVSTK_OK) {
            continue;
        }
        i2c_writef(fd,
                   "  %u: %s addr=0x%02X regs=%u max_value=%u\r\n",
                   (unsigned)i,
                   device.name,
                   (unsigned)device.addr_7b,
                   (unsigned)device.reg_count,
                   (unsigned)device.max_value_code);
    }
}

static void cmd_info(int fd,
                     size_t device_id,
                     const i2c_device_config_t *config)
{
    bvstk_i2c_master_service_t *service =
        bvstk_runtime_i2c_master_service();
    bvstk_i2c_device_t device;
    bvstk_i2c_policy_entry_t policy;

    if (service == NULL ||
        bvstk_i2c_master_service_device_info(service, device_id, &device) != BVSTK_OK ||
        bvstk_i2c_master_service_get_policy(service, device_id, &policy) != BVSTK_OK) {
        write_str(fd, "ERR (no device)\r\n");
        return;
    }
    i2c_writef(fd,
               "selected: %s addr=0x%02X regs=%u max_value=%u policy=%s\r\n",
               device.name,
               (unsigned)device.addr_7b,
               (unsigned)device.reg_count,
               (unsigned)device.max_value_code,
               policy.mode == I2C_POLICY_BLACKLIST ? "blacklist" : "whitelist");
    if (config != NULL) {
        i2c_writef(fd,
                   "file=%s whitelist_len=%u blacklist_len=%u settings_len=%u\r\n",
                   config->file_name[0] ? config->file_name : "?",
                   (unsigned)policy.whitelist_len,
                   (unsigned)policy.blacklist_len,
                   (unsigned)config->settings_len);
    }
}

static void cmd_read(int fd, size_t device_id, const char *register_text)
{
    bvstk_i2c_master_service_t *service =
        bvstk_runtime_i2c_master_service();
    bool ok = false;
    unsigned long reg = parse_num(register_text, &ok);
    uint8_t value = 0U;

    if (service == NULL || !ok || reg > 0xFFU ||
        bvstk_i2c_master_service_read(service,
                                      device_id,
                                      (uint8_t)reg,
                                      &value,
                                      100U) != BVSTK_OK) {
        write_str(fd, "ERR\r\n");
        return;
    }
    i2c_writef(fd, "OK REG 0x%02lX = 0x%02X %u\r\n", reg, value, value);
}

static void cmd_write(int fd,
                      size_t device_id,
                      const i2c_device_config_t *config,
                      const char *register_text,
                      const char *value_text)
{
    bvstk_i2c_master_service_t *service =
        bvstk_runtime_i2c_master_service();
    bool register_ok = false;
    bool value_ok = false;
    unsigned long reg = parse_num(register_text, &register_ok);
    unsigned long value = parse_num(value_text, &value_ok);
    bvstk_status_t status;

    if (service == NULL || config == NULL || !register_ok || !value_ok ||
        reg > 0xFFU || value > 0xFFU) {
        write_str(fd, "ERR\r\n");
        return;
    }
    if (reg >= config->reg_count || value > config->max_value_code) {
        write_str(fd, "ERR DENIED outside configured range\r\n");
        return;
    }
    status = bvstk_i2c_master_service_write(service,
                                            device_id,
                                            (uint8_t)reg,
                                            (uint8_t)value,
                                            BVSTK_EVENT_SOURCE_CONSOLE,
                                            100U);
    if (status == BVSTK_ERR_DENIED) {
        write_str(fd, "ERR DENIED by policy\r\n");
        return;
    }
    if (status != BVSTK_OK || !bvstk_runtime_i2c_sync_device(device_id, 1)) {
        write_str(fd, "ERR WRITE_FAILED\r\n");
        return;
    }
    write_str(fd, "OK\r\n");
}

static void cmd_policy(int fd,
                       size_t device_id,
                       const char *mode)
{
    bvstk_i2c_master_service_t *service =
        bvstk_runtime_i2c_master_service();
    i2c_policy_t policy;

    if (service == NULL || mode == NULL) {
        write_str(fd, "ERR\r\n");
        return;
    }
    if (strcasecmp(mode, "whitelist") == 0) {
        policy = I2C_POLICY_WHITELIST;
    } else if (strcasecmp(mode, "blacklist") == 0) {
        policy = I2C_POLICY_BLACKLIST;
    } else {
        write_str(fd, "ERR\r\n");
        return;
    }
    if (bvstk_i2c_master_service_set_policy(service, device_id, policy) != BVSTK_OK ||
        !bvstk_runtime_i2c_sync_device(device_id, 1)) {
        write_str(fd, "ERR\r\n");
        return;
    }
    write_str(fd, "OK\r\n");
}

static void cmd_addr(int fd,
                     const i2c_device_config_t *config,
                     const char *address_text)
{
    bool ok = false;
    unsigned long address = parse_num(address_text, &ok);
    i2c_device_config_t next;

    if (config == NULL || !ok || address > 0x7FU) {
        write_str(fd, "ERR\r\n");
        return;
    }
    next = *config;
    next.addr_7b = (uint8_t)address;
    if (persist_config(fd, &next)) {
        write_str(fd, "OK\r\n");
    }
}

static void cmd_rules(int fd,
                      const i2c_device_config_t *config,
                      const bvstk_i2c_policy_entry_t *policy)
{
    if (config == NULL || policy == NULL) {
        write_str(fd, "ERR (no device)\r\n");
        return;
    }
    i2c_writef(fd,
               "POLICY=%s\r\n",
               policy->mode == I2C_POLICY_BLACKLIST ? "BLACKLIST" : "WHITELIST");
    i2c_writef(fd, "WHITELIST (%u):\r\n", (unsigned)policy->whitelist_len);
    for (size_t i = 0U; i < policy->whitelist_len; ++i) {
        i2c_writef(fd,
                   "  { reg:0x%02X val:0x%02X }\r\n",
                   policy->whitelist[i].reg,
                   policy->whitelist[i].val);
    }
    i2c_writef(fd, "BLACKLIST (%u):\r\n", (unsigned)policy->blacklist_len);
    for (size_t i = 0U; i < policy->blacklist_len; ++i) {
        i2c_writef(fd,
                   "  { reg:0x%02X val:0x%02X }\r\n",
                   policy->blacklist[i].reg,
                   policy->blacklist[i].val);
    }
}

static void cmd_policy_show(int fd,
                            size_t device_id,
                            const i2c_device_config_t *config,
                            const char *what)
{
    bvstk_i2c_master_service_t *service =
        bvstk_runtime_i2c_master_service();
    bvstk_i2c_policy_entry_t policy;

    if (service == NULL || config == NULL ||
        bvstk_i2c_master_service_get_policy(service, device_id, &policy) != BVSTK_OK) {
        write_str(fd, "ERR (no device)\r\n");
        return;
    }
    if (what == NULL) {
        i2c_writef(fd,
                   "POLICY=%s\r\n",
                   policy.mode == I2C_POLICY_BLACKLIST ? "BLACKLIST" : "WHITELIST");
    } else if (strcasecmp(what, "rules") == 0) {
        cmd_rules(fd, config, &policy);
    } else if (strcasecmp(what, "whitelist") == 0) {
        i2c_writef(fd, "WHITELIST (%u):\r\n", (unsigned)policy.whitelist_len);
        for (size_t i = 0U; i < policy.whitelist_len; ++i) {
            i2c_writef(fd,
                       "  { reg:0x%02X val:0x%02X }\r\n",
                       policy.whitelist[i].reg,
                       policy.whitelist[i].val);
        }
    } else if (strcasecmp(what, "blacklist") == 0) {
        i2c_writef(fd, "BLACKLIST (%u):\r\n", (unsigned)policy.blacklist_len);
        for (size_t i = 0U; i < policy.blacklist_len; ++i) {
            i2c_writef(fd,
                       "  { reg:0x%02X val:0x%02X }\r\n",
                       policy.blacklist[i].reg,
                       policy.blacklist[i].val);
        }
    } else {
        write_str(fd, "ERR\r\n");
    }
}

static void cmd_policy_list_edit(int fd,
                                 size_t device_id,
                                 const i2c_device_config_t *config,
                                 const char *list_name,
                                 const char *action,
                                 const char *register_text,
                                 const char *value_text)
{
    bool register_ok = false;
    bool value_ok = false;
    unsigned long reg = parse_num(register_text, &register_ok);
    unsigned long value = parse_num(value_text, &value_ok);
    i2c_device_config_t next;

    (void)device_id;
    if (config == NULL || list_name == NULL || action == NULL) {
        write_str(fd, "ERR\r\n");
        return;
    }
    next = *config;
    if (strcasecmp(action, "clear") == 0) {
        if (strcasecmp(list_name, "whitelist") == 0) {
            next.whitelist_len = 0U;
        } else if (strcasecmp(list_name, "blacklist") == 0) {
            next.blacklist_len = 0U;
        } else {
            write_str(fd, "ERR\r\n");
            return;
        }
    } else {
        if (!register_ok || !value_ok || reg > 0xFFU || value > 0xFFU ||
            reg >= next.reg_count || value > next.max_value_code) {
            write_str(fd, "ERR\r\n");
            return;
        }
        if (strcasecmp(list_name, "whitelist") == 0) {
            if (strcasecmp(action, "add") == 0) {
                rule_add(next.whitelist, &next.whitelist_len, (uint8_t)reg, (uint8_t)value);
            } else if (strcasecmp(action, "del") == 0 ||
                       strcasecmp(action, "delete") == 0) {
                rule_remove(next.whitelist, &next.whitelist_len, (uint8_t)reg, (uint8_t)value);
            } else {
                write_str(fd, "ERR\r\n");
                return;
            }
        } else if (strcasecmp(list_name, "blacklist") == 0) {
            if (strcasecmp(action, "add") == 0) {
                rule_add(next.blacklist, &next.blacklist_len, (uint8_t)reg, (uint8_t)value);
            } else if (strcasecmp(action, "del") == 0 ||
                       strcasecmp(action, "delete") == 0) {
                rule_remove(next.blacklist, &next.blacklist_len, (uint8_t)reg, (uint8_t)value);
            } else {
                write_str(fd, "ERR\r\n");
                return;
            }
        } else {
            write_str(fd, "ERR\r\n");
            return;
        }
    }
    if (persist_config(fd, &next)) {
        write_str(fd, "OK\r\n");
    }
}

static void cmd_policy_dispatch(int fd,
                                size_t device_id,
                                const i2c_device_config_t *config,
                                char **save)
{
    char *sub = strtok_r(NULL, " \t", save);
    if (sub == NULL) {
        cmd_policy_show(fd, device_id, config, NULL);
    } else if (strcasecmp(sub, "show") == 0) {
        cmd_policy_show(fd, device_id, config, strtok_r(NULL, " \t", save));
    } else if (strcasecmp(sub, "set") == 0) {
        cmd_policy(fd, device_id, strtok_r(NULL, " \t", save));
    } else if (strcasecmp(sub, "whitelist") == 0 ||
               strcasecmp(sub, "blacklist") == 0) {
        char *action = strtok_r(NULL, " \t", save);
        if (action == NULL) {
            cmd_policy_show(fd, device_id, config, sub);
        } else {
            cmd_policy_list_edit(fd,
                                 device_id,
                                 config,
                                 sub,
                                 action,
                                 strtok_r(NULL, " \t", save),
                                 strtok_r(NULL, " \t", save));
        }
    } else {
        write_str(fd, "ERR\r\n");
    }
}

bool i2c_handle(char *token, char **save, int fd)
{
    size_t device_id = 0U;
    const i2c_device_config_t *config = NULL;
    char *subcommand;
    char *command;

    if (token == NULL || strcasecmp(token, "i2c") != 0) {
        return false;
    }
    subcommand = strtok_r(NULL, " \t", save);
    if (subcommand == NULL || strcasecmp(subcommand, "-h") == 0 ||
        strcasecmp(subcommand, "--help") == 0) {
        i2c_help(fd);
        return true;
    }
    if (strcasecmp(subcommand, "list") == 0) {
        cmd_list(fd);
        return true;
    }
    if (!parse_selector(subcommand, &device_id, &config)) {
        write_str(fd, "ERR (device not found)\r\n");
        return true;
    }
    command = strtok_r(NULL, " \t", save);
    if (command == NULL || strcasecmp(command, "info") == 0) {
        cmd_info(fd, device_id, config);
    } else if (strcasecmp(command, "r") == 0) {
        cmd_read(fd, device_id, strtok_r(NULL, " \t", save));
    } else if (strcasecmp(command, "w") == 0) {
        cmd_write(fd,
                  device_id,
                  config,
                  strtok_r(NULL, " \t", save),
                  strtok_r(NULL, " \t", save));
    } else if (strcasecmp(command, "addr") == 0 ||
               strcasecmp(command, "address") == 0) {
        cmd_addr(fd, config, strtok_r(NULL, " \t", save));
    } else if (strcasecmp(command, "policy") == 0) {
        cmd_policy_dispatch(fd, device_id, config, save);
    } else {
        write_str(fd, "ERR\r\n");
    }
    return true;
}

void i2c_help(int fd)
{
    write_str(fd, "i2c usage:\r\n");
    write_str(fd, "  i2c list\r\n");
    write_str(fd, "  i2c <name> [info]\r\n");
    write_str(fd, "  i2c <name> r <reg>\r\n");
    write_str(fd, "  i2c <name> w <reg> <val>\r\n");
    write_str(fd, "  i2c <name> addr <addr_7b>   (set 7-bit address, persists)\r\n");
    write_str(fd, "  i2c <name> policy\r\n");
    write_str(fd, "  i2c <name> policy show [rules|whitelist|blacklist]\r\n");
    write_str(fd, "  i2c <name> policy [whitelist|blacklist]  (show list)\r\n");
    write_str(fd, "  i2c <name> policy set <whitelist|blacklist>\r\n");
    write_str(fd, "  i2c <name> policy whitelist add <reg> <val>\r\n");
    write_str(fd, "  i2c <name> policy whitelist del <reg> <val>\r\n");
    write_str(fd, "  i2c <name> policy whitelist clear\r\n");
    write_str(fd, "  i2c <name> policy blacklist add <reg> <val>\r\n");
    write_str(fd, "  i2c <name> policy blacklist del <reg> <val>\r\n");
    write_str(fd, "  i2c <name> policy blacklist clear\r\n");
    write_str(fd, "  (address selector: use @0x.. instead of <name>)\r\n");
}
