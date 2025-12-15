#ifndef QSPI_FS_LAYOUT_H
#define QSPI_FS_LAYOUT_H

#include "../qspi_flash/qspi_flash.h"

/*
 * QSPI flash layout
 *
 * BOOT.bin (and any other boot images) typically live at address 0 on QSPI.
 * To avoid ever touching that region, the FatFs "disk" for QSPI is mapped to
 * a window that starts at QSPI_FS_BASE_BYTES and spans QSPI_FS_SIZE_BYTES.
 *
 * IMPORTANT: Set QSPI_FS_BASE_BYTES large enough to cover your boot image(s),
 * and keep it aligned to the erase-sector size.
 */

#ifndef QSPI_FS_BASE_BYTES
#define QSPI_FS_BASE_BYTES (8U * 1024U * 1024U) /* default: reserve first 8 MiB */
#endif

#ifndef QSPI_FS_SIZE_BYTES
#define QSPI_FS_SIZE_BYTES (QSPI_FLASH_SIZE_BYTES - QSPI_FS_BASE_BYTES)
#endif

#if (QSPI_FS_BASE_BYTES % QSPI_FLASH_SECTOR_SIZE) != 0
#error "QSPI_FS_BASE_BYTES must be aligned to QSPI_FLASH_SECTOR_SIZE"
#endif

#if (QSPI_FS_SIZE_BYTES % 512U) != 0
#error "QSPI_FS_SIZE_BYTES must be a multiple of 512 bytes"
#endif

#if (QSPI_FS_BASE_BYTES + QSPI_FS_SIZE_BYTES) > QSPI_FLASH_SIZE_BYTES
#error "QSPI FS window exceeds QSPI_FLASH_SIZE_BYTES"
#endif

#endif /* QSPI_FS_LAYOUT_H */
