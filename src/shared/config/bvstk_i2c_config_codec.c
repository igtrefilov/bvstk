#include "shared/config/bvstk_i2c_config_codec.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
    int failed;
} json_writer_t;

static const char *skip_ws(const char *cursor)
{
    while (cursor != NULL && *cursor != '\0' &&
           isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    return cursor;
}

static const char *find_key(const char *json, const char *key)
{
    char pattern[64];
    const char *cursor;
    int length;

    if (json == NULL || key == NULL) {
        return NULL;
    }
    length = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (length <= 0 || (size_t)length >= sizeof(pattern)) {
        return NULL;
    }
    cursor = json;
    while (*cursor != '\0') {
        const char *match = strstr(cursor, pattern);
        const char *value;

        if (match == NULL) {
            return NULL;
        }
        value = skip_ws(match + (size_t)length);
        if (value != NULL && *value == ':') {
            return skip_ws(value + 1);
        }
        cursor = value != NULL ? value : match + 1;
    }
    return NULL;
}

static const char *parse_string(const char *cursor,
                                char *output,
                                size_t output_capacity,
                                int *ok)
{
    size_t length = 0U;

    if (ok != NULL) {
        *ok = 0;
    }
    if (output == NULL || output_capacity == 0U) {
        return cursor;
    }
    output[0] = '\0';
    cursor = skip_ws(cursor);
    if (cursor == NULL || *cursor != '"') {
        return cursor;
    }
    ++cursor;
    while (*cursor != '\0' && *cursor != '"') {
        char value = *cursor++;

        if (value == '\\') {
            if (*cursor == '\0') {
                return cursor;
            }
            value = *cursor++;
            switch (value) {
            case 'n': value = '\n'; break;
            case 'r': value = '\r'; break;
            case 't': value = '\t'; break;
            default: break;
            }
        }
        if (length + 1U >= output_capacity) {
            return cursor;
        }
        output[length++] = value;
    }
    output[length] = '\0';
    if (*cursor != '"') {
        return cursor;
    }
    if (ok != NULL) {
        *ok = 1;
    }
    return cursor + 1;
}

static const char *parse_u32(const char *cursor, uint32_t *value, int *ok)
{
    char number[32];
    char *end = NULL;
    unsigned long parsed;

    if (ok != NULL) {
        *ok = 0;
    }
    if (value != NULL) {
        *value = 0U;
    }
    cursor = skip_ws(cursor);
    if (cursor == NULL || *cursor == '\0') {
        return cursor;
    }
    if (*cursor == '"') {
        int string_ok = 0;
        const char *next = parse_string(cursor,
                                        number,
                                        sizeof(number),
                                        &string_ok);
        if (string_ok == 0) {
            return cursor;
        }
        parsed = strtoul(number, &end, 0);
        if (end == number || *end != '\0' || parsed > UINT32_MAX) {
            return cursor;
        }
        if (value != NULL) {
            *value = (uint32_t)parsed;
        }
        if (ok != NULL) {
            *ok = 1;
        }
        return next;
    }

    parsed = strtoul(cursor, &end, 0);
    if (end == cursor || parsed > UINT32_MAX) {
        return cursor;
    }
    if (value != NULL) {
        *value = (uint32_t)parsed;
    }
    if (ok != NULL) {
        *ok = 1;
    }
    return end;
}

static const char *skip_simple_value(const char *cursor, int *ok)
{
    uint32_t ignored;

    if (ok != NULL) {
        *ok = 0;
    }
    cursor = skip_ws(cursor);
    if (cursor == NULL || *cursor == '\0') {
        return cursor;
    }
    if (*cursor == '"') {
        char ignored_string[8];
        return parse_string(cursor,
                            ignored_string,
                            sizeof(ignored_string),
                            ok);
    }
    if (strncmp(cursor, "true", 4U) == 0) {
        if (ok != NULL) *ok = 1;
        return cursor + 4;
    }
    if (strncmp(cursor, "false", 5U) == 0) {
        if (ok != NULL) *ok = 1;
        return cursor + 5;
    }
    if (strncmp(cursor, "null", 4U) == 0) {
        if (ok != NULL) *ok = 1;
        return cursor + 4;
    }
    return parse_u32(cursor, &ignored, ok);
}

static int parse_rule_array(const char *cursor,
                            i2c_rule_entry_t *rules,
                            size_t rules_capacity,
                            size_t *rules_count)
{
    size_t count = 0U;

    if (rules_count != NULL) {
        *rules_count = 0U;
    }
    cursor = skip_ws(cursor);
    if (cursor == NULL || rules == NULL || rules_count == NULL ||
        rules_capacity == 0U || *cursor != '[') {
        return 0;
    }
    ++cursor;
    for (;;) {
        uint32_t reg = 0U;
        uint32_t value = 0U;
        int have_reg = 0;
        int have_value = 0;

        cursor = skip_ws(cursor);
        if (cursor == NULL || *cursor == '\0') {
            return 0;
        }
        if (*cursor == ']') {
            *rules_count = count;
            return 1;
        }
        if (*cursor != '{') {
            return 0;
        }
        ++cursor;
        for (;;) {
            char key[24];
            int key_ok = 0;

            cursor = skip_ws(cursor);
            if (cursor == NULL || *cursor == '\0') {
                return 0;
            }
            if (*cursor == '}') {
                ++cursor;
                break;
            }
            cursor = parse_string(cursor, key, sizeof(key), &key_ok);
            if (key_ok == 0) {
                return 0;
            }
            cursor = skip_ws(cursor);
            if (cursor == NULL || *cursor != ':') {
                return 0;
            }
            ++cursor;
            if (strcmp(key, "reg") == 0) {
                cursor = parse_u32(cursor, &reg, &have_reg);
                if (have_reg == 0) return 0;
            } else if (strcmp(key, "val") == 0) {
                cursor = parse_u32(cursor, &value, &have_value);
                if (have_value == 0) return 0;
            } else {
                int value_ok = 0;
                cursor = skip_simple_value(cursor, &value_ok);
                if (value_ok == 0) return 0;
            }
            cursor = skip_ws(cursor);
            if (cursor == NULL) return 0;
            if (*cursor == ',') {
                ++cursor;
                continue;
            }
            if (*cursor == '}') {
                ++cursor;
                break;
            }
            return 0;
        }
        if (have_reg == 0 || have_value == 0 ||
            reg > UINT8_MAX || value > UINT8_MAX ||
            count >= rules_capacity) {
            return 0;
        }
        rules[count].reg = (uint8_t)reg;
        rules[count].val = (uint8_t)value;
        ++count;
        cursor = skip_ws(cursor);
        if (cursor == NULL) return 0;
        if (*cursor == ',') {
            ++cursor;
            continue;
        }
        if (*cursor == ']') {
            *rules_count = count;
            return 1;
        }
        return 0;
    }
}

bvstk_status_t bvstk_i2c_config_validate(
    const i2c_device_config_t *config)
{
    size_t index;

    if (config == NULL || config->name[0] == '\0' ||
        memchr(config->name, '\0', sizeof(config->name)) == NULL ||
        config->addr_7b > UINT8_C(0x7F) || config->reg_count == 0U ||
        config->reg_count > I2C_CFG_MAX_REG_COUNT ||
        config->max_value_code > UINT8_C(64) ||
        (config->policy != I2C_POLICY_WHITELIST &&
         config->policy != I2C_POLICY_BLACKLIST) ||
        config->whitelist_len > I2C_CFG_RULES_MAX ||
        config->blacklist_len > I2C_CFG_RULES_MAX ||
        config->settings_len > I2C_CFG_SETTINGS_MAX) {
        return BVSTK_ERR_MALFORMED;
    }
    for (index = 0U; index < config->whitelist_len; ++index) {
        if (config->whitelist[index].reg >= config->reg_count ||
            config->whitelist[index].val > config->max_value_code) {
            return BVSTK_ERR_RANGE;
        }
    }
    for (index = 0U; index < config->blacklist_len; ++index) {
        if (config->blacklist[index].reg >= config->reg_count ||
            config->blacklist[index].val > config->max_value_code) {
            return BVSTK_ERR_RANGE;
        }
    }
    for (index = 0U; index < config->settings_len; ++index) {
        if (config->settings[index].reg >= config->reg_count) {
            return BVSTK_ERR_RANGE;
        }
    }
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_config_parse_json(const char *json,
                                           i2c_device_config_t *config)
{
    const char *value;
    uint32_t number;
    int ok;
    char policy[16];

    if (json == NULL || config == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(config, 0, sizeof(*config));
    config->policy = I2C_POLICY_WHITELIST;

    value = find_key(json, "name");
    (void)parse_string(value, config->name, sizeof(config->name), &ok);
    if (value == NULL || ok == 0 || config->name[0] == '\0') {
        return BVSTK_ERR_MALFORMED;
    }
    value = find_key(json, "addr_7b");
    (void)parse_u32(value, &number, &ok);
    if (value == NULL || ok == 0 || number > UINT8_C(0x7F)) {
        return BVSTK_ERR_MALFORMED;
    }
    config->addr_7b = (uint8_t)number;

    value = find_key(json, "reg_count");
    (void)parse_u32(value, &number, &ok);
    if (value == NULL || ok == 0 || number == 0U ||
        number > I2C_CFG_MAX_REG_COUNT) {
        return BVSTK_ERR_MALFORMED;
    }
    config->reg_count = (uint16_t)number;

    value = find_key(json, "max_value_code");
    (void)parse_u32(value, &number, &ok);
    if (value == NULL || ok == 0 || number > 64U) {
        return BVSTK_ERR_MALFORMED;
    }
    config->max_value_code = (uint8_t)number;

    value = find_key(json, "policy");
    (void)parse_string(value, policy, sizeof(policy), &ok);
    if (value == NULL || ok == 0) {
        return BVSTK_ERR_MALFORMED;
    }
    if (strcasecmp(policy, "whitelist") == 0) {
        config->policy = I2C_POLICY_WHITELIST;
    } else if (strcasecmp(policy, "blacklist") == 0) {
        config->policy = I2C_POLICY_BLACKLIST;
    } else {
        return BVSTK_ERR_MALFORMED;
    }

    value = find_key(json, "whitelist");
    if (value != NULL &&
        !parse_rule_array(value,
                          config->whitelist,
                          I2C_CFG_RULES_MAX,
                          &config->whitelist_len)) {
        return BVSTK_ERR_MALFORMED;
    }
    value = find_key(json, "blacklist");
    if (value != NULL &&
        !parse_rule_array(value,
                          config->blacklist,
                          I2C_CFG_RULES_MAX,
                          &config->blacklist_len)) {
        return BVSTK_ERR_MALFORMED;
    }
    value = find_key(json, "settings");
    if (value != NULL &&
        !parse_rule_array(value,
                          config->settings,
                          I2C_CFG_SETTINGS_MAX,
                          &config->settings_len)) {
        return BVSTK_ERR_MALFORMED;
    }
    return bvstk_i2c_config_validate(config);
}

static void writer_appendf(json_writer_t *writer, const char *format, ...)
{
    va_list arguments;
    int length;

    if (writer == NULL || writer->failed != 0 || format == NULL ||
        writer->length >= writer->capacity) {
        if (writer != NULL) writer->failed = 1;
        return;
    }
    va_start(arguments, format);
    length = vsnprintf(writer->data + writer->length,
                       writer->capacity - writer->length,
                       format,
                       arguments);
    va_end(arguments);
    if (length < 0 || (size_t)length >= writer->capacity - writer->length) {
        writer->failed = 1;
        return;
    }
    writer->length += (size_t)length;
}

static void writer_append_json_string(json_writer_t *writer,
                                      const char *value)
{
    const unsigned char *cursor = (const unsigned char *)value;

    writer_appendf(writer, "\"");
    while (writer->failed == 0 && cursor != NULL && *cursor != '\0') {
        switch (*cursor) {
        case '"': writer_appendf(writer, "\\\""); break;
        case '\\': writer_appendf(writer, "\\\\"); break;
        case '\n': writer_appendf(writer, "\\n"); break;
        case '\r': writer_appendf(writer, "\\r"); break;
        case '\t': writer_appendf(writer, "\\t"); break;
        default:
            if (*cursor < 0x20U) {
                writer->failed = 1;
            } else {
                writer_appendf(writer, "%c", (int)*cursor);
            }
            break;
        }
        ++cursor;
    }
    writer_appendf(writer, "\"");
}

static void writer_append_rules(json_writer_t *writer,
                                const i2c_rule_entry_t *rules,
                                size_t rules_count)
{
    size_t index;

    for (index = 0U; index < rules_count; ++index) {
        writer_appendf(writer,
                       "    { \"reg\": %u, \"val\": %u }%s\n",
                       (unsigned)rules[index].reg,
                       (unsigned)rules[index].val,
                       index + 1U == rules_count ? "" : ",");
    }
}

bvstk_status_t bvstk_i2c_config_serialize_json(
    const i2c_device_config_t *config,
    char *json,
    size_t json_capacity,
    size_t *json_size)
{
    json_writer_t writer;
    bvstk_status_t status = bvstk_i2c_config_validate(config);

    if (json_size != NULL) {
        *json_size = 0U;
    }
    if (status != BVSTK_OK) {
        return status;
    }
    if (json == NULL || json_size == NULL || json_capacity == 0U) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(&writer, 0, sizeof(writer));
    writer.data = json;
    writer.capacity = json_capacity;
    writer_appendf(&writer, "{\n  \"name\": ");
    writer_append_json_string(&writer, config->name);
    writer_appendf(&writer,
                   ",\n  \"addr_7b\": %u,\n"
                   "  \"reg_count\": %u,\n"
                   "  \"max_value_code\": %u,\n"
                   "  \"policy\": \"%s\",\n"
                   "  \"whitelist\": [\n",
                   (unsigned)config->addr_7b,
                   (unsigned)config->reg_count,
                   (unsigned)config->max_value_code,
                   config->policy == I2C_POLICY_BLACKLIST
                       ? "blacklist" : "whitelist");
    writer_append_rules(&writer,
                        config->whitelist,
                        config->whitelist_len);
    writer_appendf(&writer, "  ],\n  \"blacklist\": [\n");
    writer_append_rules(&writer,
                        config->blacklist,
                        config->blacklist_len);
    writer_appendf(&writer, "  ],\n  \"settings\": [\n");
    writer_append_rules(&writer,
                        config->settings,
                        config->settings_len);
    writer_appendf(&writer, "  ]\n}\n");
    if (writer.failed != 0) {
        if (json_capacity != 0U) json[0] = '\0';
        return BVSTK_ERR_RANGE;
    }
    *json_size = writer.length;
    return BVSTK_OK;
}
