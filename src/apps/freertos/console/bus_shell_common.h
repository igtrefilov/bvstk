#ifndef BVSTK_BUS_SHELL_COMMON_H
#define BVSTK_BUS_SHELL_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "shared/base/bvstk_status.h"

typedef enum {
    BUS_SHELL_POLICY_MODE,
    BUS_SHELL_POLICY_RULES,
    BUS_SHELL_POLICY_WHITELIST,
    BUS_SHELL_POLICY_BLACKLIST
} bus_shell_policy_view_t;

void bus_shell_writef(int fd, const char *format, ...);
void bus_shell_result(int fd, const char *bus, bvstk_status_t status);
bvstk_status_t bus_shell_number(const char *text, unsigned long limit, unsigned long *value);
void bus_shell_changed(int fd);
void bus_shell_saved(int fd, const char *bus, const char *file_name);
void bus_shell_read_result(int fd, uint8_t reg, uint16_t value, int hex_digits);
void bus_shell_policy_mode(int fd, bool blacklist);
void bus_shell_rules_header(int fd, const char *name, size_t count);
/* A NULL mask formats an 8-bit I2C rule; a mask formats a 16-bit MDIO rule. */
void bus_shell_rule(int fd, uint8_t reg, uint16_t value, const uint16_t *mask);
/* Parse read-only policy forms. Invalid/edit forms return false, without output. */
bool bus_shell_policy_view(int argc, char *const argv[], bus_shell_policy_view_t *view);

#endif
