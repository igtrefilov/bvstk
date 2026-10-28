#ifndef BVSTK_STORAGE_CLI_H
#define BVSTK_STORAGE_CLI_H
#include "services/cli/bvstk_cli.h"
#include "services/fs/bvstk_fs_storage.h"
#include "services/control/bvstk_control_api.h"
/* Logical paths at this boundary; native OS paths stay in the adapters. */
int bvstk_storage_cli(bvstk_cli_t *, const bvstk_fs_storage_t *, const char *cwd,
    int argc, char **argv);
int bvstk_memory_cli(bvstk_cli_t *, const bvstk_control_api_t *, int, char **);
int bvstk_pl_cli(bvstk_cli_t *, bvstk_control_api_t *, int, char **);
int bvstk_tar_cli(bvstk_cli_t *, const bvstk_fs_storage_t *, const char *cwd,
    int argc, char **argv);
bvstk_fs_result_t bvstk_cli_path(const char *cwd, const char *input,
    char out[BVSTK_FS_PATH_CAPACITY]);
void bvstk_cli_fs_result(bvstk_cli_t *, bvstk_fs_result_t);
bvstk_fs_result_t bvstk_tar_extract_stream(const bvstk_fs_storage_t *,const char *,int (*)(void *,void *,size_t),void *);
#endif
