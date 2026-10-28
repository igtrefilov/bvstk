#include "services/cli/bvstk_cli.h"

#include <stdarg.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>


#include "shared/base/bvstk_parse.h"

void bvstk_cli_writef(bvstk_cli_t *cli, const char *format, ...)
{
    char local[256], *buffer = local;
    va_list args, copy;
    va_start(args, format); va_copy(copy, args);
    int length = vsnprintf(local, sizeof(local), format, args);
    va_end(args);
    if (length < 0) { va_end(copy); cli->result = 1; return; }
    if ((size_t)length >= sizeof(local)) {
        buffer = malloc((size_t)length + 1U);
        if (!buffer) { va_end(copy); cli->result = 1; return; }
        (void)vsnprintf(buffer, (size_t)length + 1U, format, copy);
    }
    va_end(copy);
    if (length && bvstk_output_write(&cli->output, buffer, (size_t)length) != length) cli->result = 1;
    if (buffer != local) free(buffer);
}

void bvstk_cli_result(bvstk_cli_t *cli, const char *bus, bvstk_status_t status)
{
    if (status != BVSTK_OK) cli->result = status == BVSTK_ERR_MALFORMED || status == BVSTK_ERR_RANGE ? 2 : 1;
    if (status == BVSTK_OK) {
        bvstk_cli_writef(cli, "OK\r\n");
    } else {
        bvstk_cli_writef(cli, "ERR %s %s\r\n", bus, bvstk_status_string(status));
    }
}

void bvstk_cli_changed(bvstk_cli_t *cli)
{
    bvstk_cli_writef(cli, "OK (RAM; use save to persist)\r\n");
}

bvstk_status_t bvstk_cli_number(const char *text, unsigned long limit, unsigned long *value)
{
    bool ok = false;

    if (text == NULL || text[0] == '-') {
        return BVSTK_ERR_MALFORMED;
    }
    *value = parse_num(text, &ok);
    if (!ok) {
        return BVSTK_ERR_MALFORMED;
    }
    return *value <= limit ? BVSTK_OK : BVSTK_ERR_RANGE;
}

void bvstk_cli_saved(bvstk_cli_t *cli, const char *bus, const char *file_name)
{
    bvstk_cli_writef(cli, "OK saved flash:/config/%s/%s\r\n", bus, file_name);
}

void bvstk_cli_read_result(bvstk_cli_t *cli, uint8_t reg, uint16_t value, int hex_digits)
{
    bvstk_cli_writef(cli, "OK reg=0x%02X val=0x%0*X dec=%u\r\n",
                    (unsigned)reg, hex_digits, (unsigned)value, (unsigned)value);
}

void bvstk_cli_policy_mode(bvstk_cli_t *cli, bool blacklist)
{
    bvstk_cli_writef(cli, "policy=%s\r\n", blacklist ? "blacklist" : "whitelist");
}

void bvstk_cli_rules_header(bvstk_cli_t *cli, const char *name, size_t count)
{
    bvstk_cli_writef(cli, "%s (%u):\r\n", name, (unsigned)count);
}

void bvstk_cli_rule(bvstk_cli_t *cli, uint8_t reg, uint16_t value, const uint16_t *mask)
{
    if (mask != NULL) {
        bvstk_cli_writef(cli, "  reg=0x%02X val=0x%04X mask=0x%04X\r\n",
                        (unsigned)reg, (unsigned)value, (unsigned)*mask);
    } else {
        bvstk_cli_writef(cli, "  reg=0x%02X val=0x%02X\r\n", (unsigned)reg, (unsigned)value);
    }
}

bool bvstk_cli_policy_view(int argc, char *const argv[], bvstk_cli_policy_view_t *view)
{
    const char *word;

    if (argc == 0 || (argc == 1 && strcasecmp(argv[0], "show") == 0)) {
        *view = BUS_SHELL_POLICY_MODE;
        return true;
    }
    if (argc == 1) {
        word = argv[0];
    } else if (argc == 2 && strcasecmp(argv[0], "show") == 0) {
        word = argv[1];
        if (strcasecmp(word, "rules") == 0) {
            *view = BUS_SHELL_POLICY_RULES;
            return true;
        }
    } else {
        return false;
    }
    if (strcasecmp(word, "whitelist") == 0) {
        *view = BUS_SHELL_POLICY_WHITELIST;
        return true;
    }
    if (strcasecmp(word, "blacklist") == 0) {
        *view = BUS_SHELL_POLICY_BLACKLIST;
        return true;
    }
    return false;
}

void bvstk_cli_text(bvstk_cli_t *cli, const char *text)
{
    size_t length = strlen(text);
    if (bvstk_output_write(&cli->output, text, length) != (int)length) cli->result = 1;
}
int bvstk_cli_tokens(char *line, char **argv, size_t capacity)
{
    size_t count = 0;
    char *input = line, *out = line;
    while (*input) {
        while (*input == ' ' || *input == '\t' || *input == '\r' || *input == '\n') ++input;
        if (!*input) break;
        if (count == capacity) return -1;
        argv[count++] = out;
        char quote = 0;
        while (*input) {
            char c = *input;
            if (!quote && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) break;
            ++input;
            if (c == quote) { quote = 0; continue; }
            if (!quote && (c == '\'' || c == '"')) { quote = c; continue; }
            if (c == '\\' && quote != '\'') {
                if (!*input) return -1;
                c = *input++;
            }
            *out++ = c;
        }
        if (quote) return -1;
        if (*input) ++input;
        *out++ = '\0';
    }
    return (int)count;
}

int bvstk_cli_buffer_write(void *context, const void *data, size_t size)
{
    bvstk_cli_buffer_t *b = context;
    if (!b || !b->data || !b->capacity || b->truncated) return -1;
    if (size >= b->capacity - b->length) {
        b->truncated = true;
        const char error[] = "ERR output exceeds response capacity\r\n";
        size_t n = sizeof(error) - 1U;
        if (b->capacity > n) {
            size_t offset = b->length;
            if (offset + n >= b->capacity) offset = b->capacity - n - 1U;
            memcpy(b->data + offset, error, n);
            b->length = offset + n; b->data[b->length] = '\0';
        }
        return -1;
    }
    memcpy(b->data + b->length, data, size);
    b->length += size; b->data[b->length] = '\0';
    return (int)size;
}
