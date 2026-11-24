#ifndef BVSTK_FATFS_H
#define BVSTK_FATFS_H

#include "ff.h"

/* Mount and unmount serialize with native file and directory operations. */
FRESULT bvstk_fatfs_mount(FATFS *fs, const TCHAR *path, BYTE immediate);

#endif
