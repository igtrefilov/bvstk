#ifndef QSPI_FLASH_H
#define QSPI_FLASH_H

#include "xil_types.h"

/* Mirror the hardware geometry that diskio needs. */
#define QSPI_FLASH_PAGE_SIZE     256U
#define QSPI_FLASH_SECTOR_SIZE   0x1000U
#define QSPI_FLASH_SIZE_BYTES    (32U * 1024U * 1024U)

int qspi_flash_init(void);
int qspi_flash_read(u32 address, u8 *data, u32 length);
int qspi_flash_program(u32 address, const u8 *data, u32 length);
int qspi_flash_erase_sector(u32 address);
int qspi_flash_self_test(void);

#endif /* QSPI_FLASH_H */
