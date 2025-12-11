#ifndef QSPI_FS_H
#define QSPI_FS_H

#include "../fs/fs_shared.h"

#define QSPI_ROOT "1:/"

int start_qspi_fs(void);
fs_shared_ctx_t *qspi_fs_get_context(void);
int qspi_fs_is_ready(void);

#endif // QSPI_FS_H
