#ifndef BVSTK_AX7020_QSPI_LAYOUT_H
#define BVSTK_AX7020_QSPI_LAYOUT_H

/* AX7020 QSPI flash geometry. */
#define QSPI_FLASH_PAGE_SIZE     256U
#define QSPI_FLASH_SECTOR_SIZE   0x1000U
#define QSPI_FLASH_SIZE_BYTES    (32U * 1024U * 1024U)

/*
 * BOOT.bin and other boot images normally start at address zero. Keep the
 * FatFs window above that reserved area and aligned to an erase sector.
 */
#ifndef QSPI_FS_BASE_BYTES
#define QSPI_FS_BASE_BYTES (8U * 1024U * 1024U)
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

#endif /* BVSTK_AX7020_QSPI_LAYOUT_H */
