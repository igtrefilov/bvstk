#ifndef BVSTK_SYSTEM_NEUTRINO_H
#define BVSTK_SYSTEM_NEUTRINO_H
#include "services/system/bvstk_system.h"
#include "apps/neutrino/commands/bvstk_command.h"
int bvstk_system_neutrino_start(bvstk_neutrino_command_t *);
int bvstk_system_neutrino_command(int, char **, const bvstk_output_t *);
#include "shared/protocols/http/bvstk_http.h"
int bvstk_system_neutrino_http(const http_request_t *,http_conn_t *);
#endif
