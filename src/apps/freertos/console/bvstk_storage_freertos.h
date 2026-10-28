#ifndef BVSTK_STORAGE_FREERTOS_H
#define BVSTK_STORAGE_FREERTOS_H
#include "apps/freertos/console/console_common.h"
#include "services/cli/bvstk_storage_cli.h"
void bvstk_storage_freertos_cwd(const console_session_t *, char out[BVSTK_FS_PATH_CAPACITY]);
#endif
