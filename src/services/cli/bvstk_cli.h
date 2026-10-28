#ifndef BVSTK_CLI_H
#define BVSTK_CLI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "shared/base/bvstk_status.h"
#include "shared/interfaces/bvstk_output.h"
#include "services/i2c/bvstk_i2c_master_service.h"
#include "services/mdio/bvstk_mdio_service.h"
#include "services/stream/bvstk_stream_sim.h"

typedef struct {
    bvstk_output_t output;
    void *context;
    bvstk_i2c_master_service_t *i2c;
    bvstk_mdio_service_t *mdio;
    const i2c_device_config_t *(*i2c_config)(void *, size_t id);
    bvstk_status_t (*i2c_apply)(void *, size_t id, const i2c_device_config_t *);
    bvstk_status_t (*i2c_sync)(void *, size_t id);
    bvstk_status_t (*mdio_changed)(void *, size_t id);
    bvstk_status_t (*save)(void *, bool mdio, size_t id);
    bvstk_stream_sim_t *streams;
    int result;
} bvstk_cli_t;

typedef struct {
    char *data;
    size_t capacity, length;
    bool truncated;
} bvstk_cli_buffer_t;
int bvstk_cli_buffer_write(void *, const void *, size_t);


typedef enum {
    BUS_SHELL_POLICY_MODE,
    BUS_SHELL_POLICY_RULES,
    BUS_SHELL_POLICY_WHITELIST,
    BUS_SHELL_POLICY_BLACKLIST
} bvstk_cli_policy_view_t;

void bvstk_cli_writef(bvstk_cli_t *cli, const char *format, ...);
void bvstk_cli_result(bvstk_cli_t *cli, const char *bus, bvstk_status_t status);
bvstk_status_t bvstk_cli_number(const char *text, unsigned long limit, unsigned long *value);
void bvstk_cli_changed(bvstk_cli_t *cli);
void bvstk_cli_saved(bvstk_cli_t *cli, const char *bus, const char *file_name);
void bvstk_cli_read_result(bvstk_cli_t *cli, uint8_t reg, uint16_t value, int hex_digits);
void bvstk_cli_policy_mode(bvstk_cli_t *cli, bool blacklist);
void bvstk_cli_rules_header(bvstk_cli_t *cli, const char *name, size_t count);
/* A NULL mask formats an 8-bit I2C rule; a mask formats a 16-bit MDIO rule. */
void bvstk_cli_rule(bvstk_cli_t *cli, uint8_t reg, uint16_t value, const uint16_t *mask);
/* Parse read-only policy forms. Invalid/edit forms return false, without output. */
bool bvstk_cli_policy_view(int argc, char *const argv[], bvstk_cli_policy_view_t *view);

void bvstk_cli_text(bvstk_cli_t *, const char *);
bool bvstk_i2c_cli(bvstk_cli_t *, int argc, char **argv);
bool bvstk_mdio_cli(bvstk_cli_t *, int argc, char **argv);
void bvstk_i2c_cli_help(bvstk_cli_t *);
void bvstk_mdio_cli_help(bvstk_cli_t *);
bool bvstk_stream_cli(bvstk_cli_t *, uint8_t service, int argc, char **argv);
void bvstk_stream_cli_help(bvstk_cli_t *, uint8_t service);
int bvstk_cli_tokens(char *line, char **argv, size_t capacity);
#endif
