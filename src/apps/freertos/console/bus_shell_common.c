#include "apps/freertos/console/bus_shell_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <strings.h>

#include "apps/freertos/console/console_stream.h"
#include "shared/base/bvstk_parse.h"

void bus_shell_writef(int fd, const char *format, ...)
{
    char buffer[256];
    va_list args;
    int length;

    va_start(args, format);
    length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (length > 0) {
        size_t count = (size_t)length < sizeof(buffer) ? (size_t)length : sizeof(buffer) - 1U;
        (void)console_stream_write(fd, buffer, count);
    }
}

void bus_shell_result(int fd, const char *bus, bvstk_status_t status)
{
    if (status == BVSTK_OK) {
        bus_shell_writef(fd, "OK\r\n");
    } else {
        bus_shell_writef(fd, "ERR %s %s\r\n", bus, bvstk_status_string(status));
    }
}

void bus_shell_changed(int fd)
{
    bus_shell_writef(fd, "OK (RAM; use save to persist)\r\n");
}

bvstk_status_t bus_shell_number(const char *text, unsigned long limit, unsigned long *value)
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

void bus_shell_saved(int fd, const char *bus, const char *file_name)
{
    bus_shell_writef(fd, "OK saved flash:/config/%s/%s\r\n", bus, file_name);
}

void bus_shell_read_result(int fd, uint8_t reg, uint16_t value, int hex_digits)
{
    bus_shell_writef(fd, "OK reg=0x%02X val=0x%0*X dec=%u\r\n",
                    (unsigned)reg, hex_digits, (unsigned)value, (unsigned)value);
}

void bus_shell_policy_mode(int fd, bool blacklist)
{
    bus_shell_writef(fd, "policy=%s\r\n", blacklist ? "blacklist" : "whitelist");
}

void bus_shell_rules_header(int fd, const char *name, size_t count)
{
    bus_shell_writef(fd, "%s (%u):\r\n", name, (unsigned)count);
}

void bus_shell_rule(int fd, uint8_t reg, uint16_t value, const uint16_t *mask)
{
    if (mask != NULL) {
        bus_shell_writef(fd, "  reg=0x%02X val=0x%04X mask=0x%04X\r\n",
                        (unsigned)reg, (unsigned)value, (unsigned)*mask);
    } else {
        bus_shell_writef(fd, "  reg=0x%02X val=0x%02X\r\n", (unsigned)reg, (unsigned)value);
    }
}

bool bus_shell_policy_view(int argc, char *const argv[], bus_shell_policy_view_t *view)
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
