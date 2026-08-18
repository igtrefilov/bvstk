#ifndef BVSTK_PL_SD_REGS_H
#define BVSTK_PL_SD_REGS_H

#include <stdint.h>

/* Byte offsets in the AXI4-Lite register window of sd_spi_controller. */
#define BVSTK_SD_CONTROL_OFFSET       UINT32_C(0x00)
#define BVSTK_SD_STATUS_OFFSET        UINT32_C(0x04)
#define BVSTK_SD_BLOCK_OFFSET         UINT32_C(0x08)
#define BVSTK_SD_DATA_OFFSET          UINT32_C(0x0c)
#define BVSTK_SD_BUFFER_INDEX_OFFSET  UINT32_C(0x10)
#define BVSTK_SD_CLOCK_DIV_OFFSET     UINT32_C(0x14)

#define BVSTK_SD_CONTROL_INIT         (UINT32_C(1) << 0)
#define BVSTK_SD_CONTROL_READ         (UINT32_C(1) << 1)
#define BVSTK_SD_CONTROL_WRITE        (UINT32_C(1) << 2)
#define BVSTK_SD_CONTROL_CLEAR        (UINT32_C(1) << 3)

#define BVSTK_SD_STATUS_BUSY          (UINT32_C(1) << 0)
#define BVSTK_SD_STATUS_DONE          (UINT32_C(1) << 1)
#define BVSTK_SD_STATUS_ERROR         (UINT32_C(1) << 2)
#define BVSTK_SD_STATUS_INITIALIZED   (UINT32_C(1) << 3)
#define BVSTK_SD_STATUS_HIGH_CAPACITY (UINT32_C(1) << 4)
#define BVSTK_SD_STATUS_INIT_ACTIVE  (UINT32_C(1) << 5)
#define BVSTK_SD_STATUS_ERROR_SHIFT   8U
#define BVSTK_SD_STATUS_ERROR_MASK    UINT32_C(0x0000ff00)

#endif /* BVSTK_PL_SD_REGS_H */
