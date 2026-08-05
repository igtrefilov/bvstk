#ifndef BVSTK_FREERTOS_XILINX_QSPI_FLASH_H
#define BVSTK_FREERTOS_XILINX_QSPI_FLASH_H

#include "xil_types.h"
#include "hardware/boards/ax7020/bvstk_qspi_layout.h"

int qspi_flash_init(void);
int qspi_flash_read(u32 address, u8 *data, u32 length);
int qspi_flash_program(u32 address, const u8 *data, u32 length);
int qspi_flash_erase_sector(u32 address);
int qspi_flash_self_test(void);

#endif /* BVSTK_FREERTOS_XILINX_QSPI_FLASH_H */
