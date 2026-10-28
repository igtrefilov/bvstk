#include "services/cli/bvstk_cli.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

static bool eq(const char *a, const char *b)
{
    return a != NULL && strcasecmp(a, b) == 0;
}

static void result(bvstk_cli_t *cli, bvstk_status_t status)
{
    bvstk_cli_result(cli, "i2c", status);
}

static bvstk_status_t number(const char *text, unsigned long limit, unsigned long *value)
{
    return bvstk_cli_number(text, limit, value);
}

static bvstk_status_t register_number(const i2c_device_config_t *config,
    const char *text, unsigned long *value)
{
    if (!config->endpoint.space_count) return number(text, config->reg_count - 1U, value);
    const char *colon = strchr(text, ':');
    if (!colon) return BVSTK_ERR_MALFORMED;
    for (unsigned i = 0; i < config->endpoint.space_count; ++i) {
        const bvstk_address_space_t *s = &config->endpoint.spaces[i];
        if (strlen(s->id) == (size_t)(colon - text) && !strncmp(s->id, text, (size_t)(colon - text))) {
            bvstk_status_t st = number(colon + 1, s->count - 1U, value);
            if (st == BVSTK_OK) *value += s->first;
            return st;
        }
    }
    return BVSTK_ERR_NOT_FOUND;
}

static bvstk_status_t select_device(bvstk_cli_t *cli, const char *token, size_t *id,
                                    const i2c_device_config_t **config)
{
    bvstk_i2c_master_service_t *service = cli->i2c;
    bvstk_status_t status;
    unsigned long address;

    if (token[0] != '@' &&
        bvstk_i2c_master_service_find_by_name(service, token, id) == BVSTK_OK) {
        *config = cli->i2c_config ? cli->i2c_config(cli->context, *id) : NULL;
    } else {
        if (number(token[0] == '@' ? token + 1 : token, 0x7FU, &address) != BVSTK_OK) {
            return BVSTK_ERR_NOT_FOUND;
        }
        status = bvstk_i2c_master_service_find_by_addr(service, (uint8_t)address, id);
        if (status != BVSTK_OK) {
            return status;
        }
        *config = cli->i2c_config ? cli->i2c_config(cli->context, *id) : NULL;
    }
    return *config != NULL ? BVSTK_OK : BVSTK_ERR_NOT_FOUND;
}

static bvstk_status_t apply_config(bvstk_cli_t *cli, const i2c_device_config_t *config)
{
    size_t id;
    bvstk_status_t st = bvstk_i2c_master_service_find_by_name(cli->i2c, config->name, &id);
    return st != BVSTK_OK ? st : cli->i2c_apply ?
        cli->i2c_apply(cli->context, id, config) : BVSTK_ERR_UNSUPPORTED;
}

static void cmd_list(bvstk_cli_t *cli, bvstk_i2c_master_service_t *service)
{
    size_t count = bvstk_i2c_master_service_device_count(service);

    bvstk_cli_writef(cli, "i2c: devices=%u\r\n", (unsigned)count);
    for (size_t i = 0U; i < count; ++i) {
        bvstk_i2c_device_t device;
        bvstk_status_t status = bvstk_i2c_master_service_device_info(service, i, &device);
        if (status != BVSTK_OK) {
            result(cli, status);
            return;
        }
        bvstk_cli_writef(cli, "  name=%s addr=0x%02X regs=%u max_value=%u\r\n",
                         device.name, (unsigned)device.addr_7b,
                         (unsigned)device.reg_count, (unsigned)device.max_value_code);
    }
}

static void cmd_info(bvstk_cli_t *cli, size_t id, const i2c_device_config_t *config)
{
    bvstk_i2c_master_service_t *service = cli->i2c;
    bvstk_i2c_device_t device;
    bvstk_i2c_policy_entry_t policy;
    bvstk_status_t status = bvstk_i2c_master_service_device_info(service, id, &device);

    if (status == BVSTK_OK) {
        status = bvstk_i2c_master_service_get_policy(service, id, &policy);
    }
    if (status != BVSTK_OK) {
        result(cli, status);
        return;
    }
    bvstk_cli_writef(cli, "name=%s addr=0x%02X regs=%u max_value=%u policy=%s\r\n",
                     device.name, (unsigned)device.addr_7b,
                     (unsigned)device.reg_count, (unsigned)device.max_value_code,
                     policy.mode == I2C_POLICY_BLACKLIST ? "blacklist" : "whitelist");
    bvstk_cli_writef(cli, "file=%s whitelist=%u blacklist=%u settings=%u\r\n",
                     config->file_name, (unsigned)policy.whitelist_len,
                     (unsigned)policy.blacklist_len, (unsigned)config->settings_len);
    bvstk_i2c_presence_t presence;
    if (bvstk_i2c_master_service_device_presence(service, id, &presence) == BVSTK_OK) {
        unsigned valid = 0, physical = 0;
        for (unsigned reg = 0; reg < device.reg_count; ++reg) {
            bvstk_i2c_cache_entry_t entry;
            if (bvstk_i2c_master_service_cache_entry(service, id, (uint8_t)reg, &entry) == BVSTK_OK && entry.valid) {
                ++valid; if (!entry.seeded) ++physical;
            }
        }
        bvstk_cli_writef(cli, "presence=%s cache_valid=%u cache_physical=%u\r\n",
            presence == BVSTK_I2C_PRESENCE_RESPONDING ? "responding" :
            presence == BVSTK_I2C_PRESENCE_UNAVAILABLE ? "unavailable" : "unknown", valid, physical);
    }
    if (device.endpoint.parent[0])
        bvstk_cli_writef(cli, "controller=pl_i2c_0 parent=%s channel=%u\r\n",
                         device.endpoint.parent, (unsigned)device.endpoint.channel);
    else bvstk_cli_writef(cli, "controller=pl_i2c_0 parent=root\r\n");
    if (device.endpoint.transfer.configured) {
        const bvstk_i2c_transfer_t *t = &device.endpoint.transfer;
        bvstk_cli_writef(cli, "address_bytes=%u address_base=0x%04X order=%s write_cycle_ms=%u verify=%s\r\n",
            t->address_bytes, t->address_base, t->little_endian ? "little" : "big",
            t->write_cycle_ms, t->verify_readback ? "readback" : "none");
    }
    if (device.endpoint.router.channel_count)
        bvstk_cli_writef(cli, "channels=%u control_register=0x%02X select_mask=0x%02X\r\n",
            device.endpoint.router.channel_count, device.endpoint.router.reg, device.endpoint.router.select_mask);
    for (unsigned i = 0; i < device.endpoint.space_count; ++i) {
        const bvstk_address_space_t *s = &device.endpoint.spaces[i];
        bvstk_cli_writef(cli, "space=%s addr=0x%02X regs=%u\r\n", s->id, s->address, s->count);
    }
}

static void cmd_read(bvstk_cli_t *cli, size_t id, const i2c_device_config_t *config, const char *text)
{
    unsigned long reg;
    uint8_t value;
    bvstk_status_t status = register_number(config, text, &reg);

    if (status == BVSTK_OK) {
        status = bvstk_i2c_master_service_read_from(cli->i2c,
            id, (uint8_t)reg, &value, BVSTK_EVENT_SOURCE_CONSOLE, 100U);
    }
    if (status == BVSTK_OK) {
        if (config->endpoint.space_count) bvstk_cli_writef(cli, "%s=0x%02X\r\n", text, value);
        else bvstk_cli_read_result(cli, (uint8_t)reg, value, 2);
    } else {
        result(cli, status);
    }
}

static void cmd_write(bvstk_cli_t *cli, size_t id, const i2c_device_config_t *config,
                      const char *reg_text, const char *value_text)
{
    unsigned long reg, value;
    bvstk_status_t status = register_number(config, reg_text, &reg);

    if (status == BVSTK_OK) {
        status = number(value_text, config->max_value_code, &value);
    }
    if (status == BVSTK_OK) {
        status = bvstk_i2c_master_service_write(cli->i2c,
                                               id, (uint8_t)reg, (uint8_t)value,
                                               BVSTK_EVENT_SOURCE_CONSOLE, 100U);
        if (status == BVSTK_OK && (!cli->i2c_sync || cli->i2c_sync(cli->context, id) != BVSTK_OK)) {
            status = BVSTK_ERR_INTERNAL;
        }
    }
    result(cli, status);
}

static void rules_show(bvstk_cli_t *cli, const char *name, const i2c_rule_entry_t *rules, size_t count,
                       const bvstk_i2c_endpoint_t *endpoint)
{
    bvstk_cli_rules_header(cli, name, count);
    for (size_t i = 0U; i < count; ++i) {
        const bvstk_address_space_t *s = bvstk_i2c_register_space(endpoint, rules[i].reg);
        if (s) bvstk_cli_writef(cli, "  reg=%s:0x%02X val=0x%02X\r\n",
            s->id, rules[i].reg - s->first, rules[i].val);
        else bvstk_cli_rule(cli, rules[i].reg, rules[i].val, NULL);
    }
}

static void policy_show(bvstk_cli_t *cli, size_t id, bvstk_cli_policy_view_t view)
{
    bvstk_i2c_policy_entry_t policy;
    bvstk_i2c_device_t device;
    bvstk_status_t status = bvstk_i2c_master_service_get_policy(
        cli->i2c, id, &policy);

    if (status == BVSTK_OK) status = bvstk_i2c_master_service_device_info(cli->i2c, id, &device);
    if (status != BVSTK_OK) {
        result(cli, status);
        return;
    }
    if (view == BUS_SHELL_POLICY_MODE || view == BUS_SHELL_POLICY_RULES) {
        bvstk_cli_policy_mode(cli, policy.mode == I2C_POLICY_BLACKLIST);
    }
    if (view == BUS_SHELL_POLICY_RULES || view == BUS_SHELL_POLICY_WHITELIST) {
        rules_show(cli, "whitelist", policy.whitelist, policy.whitelist_len, &device.endpoint);
    }
    if (view == BUS_SHELL_POLICY_RULES || view == BUS_SHELL_POLICY_BLACKLIST) {
        rules_show(cli, "blacklist", policy.blacklist, policy.blacklist_len, &device.endpoint);
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
    status = register_number(config, argv[2], &reg);
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

static void cmd_policy(bvstk_cli_t *cli, size_t id, const i2c_device_config_t *config,
                        int argc, char **argv)
{
    bvstk_cli_policy_view_t view;
    bvstk_status_t status;
    i2c_device_config_t next;

    if (bvstk_cli_policy_view(argc, argv, &view)) {
        policy_show(cli, id, view);
        return;
    }
    next = *config;
    if (argc == 2 && eq(argv[0], "set")) {
        if (eq(argv[1], "whitelist")) {
            next.policy = I2C_POLICY_WHITELIST;
        } else if (eq(argv[1], "blacklist")) {
            next.policy = I2C_POLICY_BLACKLIST;
        } else {
            result(cli, BVSTK_ERR_MALFORMED);
            return;
        }
        status = apply_config(cli, &next);
    } else {
        status = edit_rules(&next, argc, argv);
        if (status == BVSTK_OK) {
            status = apply_config(cli, &next);
        }
    }
    if (status == BVSTK_OK) {
        bvstk_cli_changed(cli);
    } else {
        result(cli, status);
    }
}

static void cmd_save(bvstk_cli_t *cli, size_t id, const char *name)
{
    (void)name;
    bvstk_status_t st = cli->save ? cli->save(cli->context, false, id) : BVSTK_ERR_UNSUPPORTED;
    const i2c_device_config_t *config = cli->i2c_config ? cli->i2c_config(cli->context, id) : NULL;
    if (st != BVSTK_OK) result(cli, st);
    else if (!config) result(cli, BVSTK_ERR_NOT_FOUND);
    else bvstk_cli_saved(cli, "i2c", config->file_name);
}

/* Keep the large v2 configuration copy out of the dispatcher frame: save
 * already needs a snapshot and runtime cache synchronization on the stack. */
static void cmd_address(bvstk_cli_t *cli, const i2c_device_config_t *config, const char *text)
{
    unsigned long address;
    i2c_device_config_t next = *config;
    bvstk_status_t status = number(text, 0x7FU, &address);
    if (status == BVSTK_OK) {
        next.addr_7b = (uint8_t)address;
        status = apply_config(cli, &next);
    }
    if (status == BVSTK_OK) bvstk_cli_changed(cli);
    else result(cli, status);
}

bool bvstk_i2c_cli(bvstk_cli_t *cli, int argc, char **argv)
{
    size_t id = 0;
    const i2c_device_config_t *config = NULL;
    bvstk_i2c_master_service_t *service;
    bvstk_status_t status;
    if (argc && eq(argv[0], "stream")) return bvstk_stream_cli(cli, DCP2_STREAM_SERVICE_I2C, argc - 1, argv + 1);
    if (argc == 0 || (argc == 1 && (eq(argv[0], "-h") || eq(argv[0], "--help")))) {
        bvstk_i2c_cli_help(cli);
        return true;
    }
    service = cli->i2c;
    if (service == NULL) {
        result(cli, BVSTK_ERR_NOT_READY);
        return true;
    }
    if (argc == 1 && eq(argv[0], "list")) {
        cmd_list(cli, service);
        return true;
    }
    status = select_device(cli, argv[0], &id, &config);
    if (status != BVSTK_OK) {
        result(cli, status);
        return true;
    }
    char qualified[48];
    if (config->endpoint.space_count && !eq(argv[0], config->name)) {
        if (argc >= 2 && eq(argv[1], "policy")) {
            cli->result = 2;
            bvstk_cli_text(cli, "Use the device name and <space>:<reg> for multi-space policies.\r\n");
            return true;
        }
        if (argc >= 3 && (eq(argv[1], "r") || eq(argv[1], "read") || eq(argv[1], "w") || eq(argv[1], "write"))) {
            unsigned long address;
            if (number(argv[0][0] == '@' ? argv[0] + 1 : argv[0], 127, &address) != BVSTK_OK) {
                result(cli, BVSTK_ERR_RANGE); return true;
            }
            int s = bvstk_i2c_address_space(&config->endpoint, config->addr_7b, address);
            if (s < 0 || strchr(argv[2], ':') || strlen(argv[2]) > 12U) {
                result(cli, BVSTK_ERR_MALFORMED); return true;
            }
            snprintf(qualified, sizeof(qualified), "%s:%s", config->endpoint.spaces[s].id, argv[2]);
            argv[2] = qualified;
        }
    }
    if (argc == 1 || (argc == 2 && eq(argv[1], "info"))) {
        cmd_info(cli, id, config);
    } else if (argc == 3 && (eq(argv[1], "r") || eq(argv[1], "read"))) {
        cmd_read(cli, id, config, argv[2]);
    } else if (argc == 4 && (eq(argv[1], "w") || eq(argv[1], "write"))) {
        cmd_write(cli, id, config, argv[2], argv[3]);
    } else if (argc == 3 && (eq(argv[1], "addr") || eq(argv[1], "address"))) {
        cmd_address(cli, config, argv[2]);
    } else if (argc == 2 && eq(argv[1], "save")) {
        cmd_save(cli, id, config->name);
    } else if (eq(argv[1], "policy")) {
        cmd_policy(cli, id, config, argc - 2, argv + 2);
    } else {
        result(cli, BVSTK_ERR_MALFORMED);
    }
    return true;
}

void bvstk_i2c_cli_help(bvstk_cli_t *cli)
{
    bvstk_cli_text(cli,
        "i2c list | -h | --help\r\n"
        "i2c <name|@addr|addr> [info]\r\n"
        "i2c <sel> r <reg> | w <reg> <value>\r\n"
        "Multi-space device: <reg> is <space>:<offset>; @addr selects a space for r/w.\r\n"
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
