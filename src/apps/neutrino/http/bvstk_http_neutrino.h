#ifndef BVSTK_HTTP_NEUTRINO_H
#define BVSTK_HTTP_NEUTRINO_H
#include "apps/neutrino/commands/bvstk_command.h"
#include "services/observation/bvstk_observation.h"
int bvstk_http_neutrino_start(bvstk_neutrino_command_t *,bvstk_observation_t *);
void start_http_server(void);
#endif
