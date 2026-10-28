#ifndef BVSTK_SYSTEM_H
#define BVSTK_SYSTEM_H
#include "shared/config/bvstk_config_model.h"
#include "shared/interfaces/bvstk_output.h"
#include "shared/base/bvstk_status.h"

typedef struct {
    bool i2c, mdio, presence_poll, acquisition, cache, observe;
    bool dcp2_fs, dcp2_notify, dcp2_stream, http, sd_pl, network_config, reboot;
} bvstk_capabilities_t;
typedef struct {
    void *context;
    bvstk_status_t (*current)(void *, network_config_t *, bool *up, bool *link);
    bvstk_status_t (*load)(void *, network_config_t *);
    bvstk_status_t (*save)(void *, const network_config_t *);
    bvstk_status_t (*apply)(void *, const network_config_t *);
    bvstk_status_t (*reboot)(void *, unsigned delay_ms);
    uint64_t (*uptime)(void *);
    /* Produces an OS-specific JSON object and actual subsystem readiness. */
    int (*details)(void *, char *, size_t, bool *, bool *);
    const char *os;
    bvstk_capabilities_t capabilities;
} bvstk_system_port_t;
/* Caller serializes each complete operation. No bus/cache guard is required. */
typedef struct {
    bvstk_system_port_t port;
    network_config_t configured;
    bool configured_valid, saved, saved_known;
} bvstk_system_t;
typedef struct { bool saved, applied; bvstk_status_t save_status, apply_status; } bvstk_network_result_t;
int bvstk_network_ipv4(const char *, uint32_t *);
int bvstk_network_mac(const char *, uint8_t [6]);
void bvstk_network_address(uint32_t, char [16]);
bvstk_status_t bvstk_network_validate(const network_config_t *);
bvstk_status_t bvstk_network_parse(const char *, network_config_t *);
int bvstk_network_format(const network_config_t *, char *, size_t);
void bvstk_system_init(bvstk_system_t *, const bvstk_system_port_t *);
bvstk_status_t bvstk_system_restore(bvstk_system_t *);
bvstk_status_t bvstk_system_network_set(bvstk_system_t *, const network_config_t *, bool apply, bvstk_network_result_t *);
int bvstk_system_runtime_json(bvstk_system_t *, char *, size_t);
int bvstk_system_network_json(bvstk_system_t *, char *, size_t);
int bvstk_system_cli(bvstk_system_t *, int argc, char **argv, const bvstk_output_t *);
#endif
