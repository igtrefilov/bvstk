#ifndef BVSTK_NEUTRINO_COMMAND_H
#define BVSTK_NEUTRINO_COMMAND_H
#include "apps/neutrino/bvstkd/bvstk_dcp2_runtime.h"
#include "services/cli/bvstk_storage_cli.h"

typedef struct {
    bvstk_neutrino_dcp2_runtime_t *runtime;
    bvstk_control_api_t *control;
    bvstk_stream_sim_t *streams;
    const bvstk_fs_storage_t *storage;
    int (*system)(int, char **, const bvstk_output_t *);
} bvstk_neutrino_command_t;
int bvstk_neutrino_command_execute(bvstk_neutrino_command_t *, const char *cwd,
    const char *line, const bvstk_output_t *);
#endif
