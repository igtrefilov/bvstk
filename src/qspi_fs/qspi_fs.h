#ifndef QSPI_FS_H
#define QSPI_FS_H

#include "../fs/fs_shared.h"

#include "xparameters.h"

#define QSPI_STR_IMPL(x) #x
#define QSPI_STR(x) QSPI_STR_IMPL(x)

#ifndef XPAR_XSDPS_NUM_INSTANCES
#define XPAR_XSDPS_NUM_INSTANCES 1
#endif

/*
 * Keep the QSPI FatFs logical drive number consistent with diskio.c:
 * DISKIO_QSPI_PDRV == DISKIO_SD_PDRV_COUNT == XPAR_XSDPS_NUM_INSTANCES.
 */
#define QSPI_ROOT QSPI_STR(XPAR_XSDPS_NUM_INSTANCES) ":/"

int start_qspi_fs(void);
fs_shared_ctx_t *qspi_fs_get_context(void);
int qspi_fs_is_ready(void);

#endif // QSPI_FS_H
