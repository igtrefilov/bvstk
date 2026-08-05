#ifndef BVSTK_AX7020_HW_CONFIG_H
#define BVSTK_AX7020_HW_CONFIG_H

#include <stddef.h>
#include <stdint.h>

/*
 * AX7020 Burevestnik PL contract exported by artifacts/fpga/design.xsa.
 *
 * This file is the single source of truth shared by the FreeRTOS and
 * Neutrino builds. The FreeRTOS/Xilinx port validates these constants against
 * xparameters.h in ports/freertos-xilinx/board/ax7020/bvstk_hw_validate.c.
 */

#define BVSTK_I2C_BRAM_BASE       UINT32_C(0x40000000)
#define BVSTK_I2C_BRAM_SIZE       UINT32_C(0x00002000)

#define BVSTK_SMI_BRAM_BASE       UINT32_C(0x42000000)
#define BVSTK_SMI_BRAM_SIZE       UINT32_C(0x00002000)

#define BVSTK_SPI_BRAM_BASE       UINT32_C(0x44000000)
#define BVSTK_SPI_BRAM_SIZE       UINT32_C(0x00002000)

#define BVSTK_I2C_MASTER_BASE     UINT32_C(0x43C00000)
#define BVSTK_I2C_MASTER_SIZE     UINT32_C(0x00010000)

#define BVSTK_SMI_MASTER_BASE     UINT32_C(0x43C10000)
#define BVSTK_SMI_MASTER_SIZE     UINT32_C(0x00010000)

#define BVSTK_SMI_SLAVE_BASE      UINT32_C(0x43C20000)
#define BVSTK_SMI_SLAVE_SIZE      UINT32_C(0x00010000)

#define BVSTK_SPI_MASTER_BASE     UINT32_C(0x43C30000)
#define BVSTK_SPI_MASTER_SIZE     UINT32_C(0x00010000)

#define BVSTK_I2C_SLAVE_BASE      UINT32_C(0x43C40000)
#define BVSTK_I2C_SLAVE_SIZE      UINT32_C(0x00010000)

/* ARM GIC interrupt IDs used by the fabric interrupt inputs. */
#define BVSTK_IRQ_SMI_MASTER      UINT32_C(61)
#define BVSTK_IRQ_SMI_SLAVE       UINT32_C(62)
#define BVSTK_IRQ_I2C_MASTER      UINT32_C(63)
#define BVSTK_IRQ_I2C_SLAVE       UINT32_C(64)
#define BVSTK_IRQ_SPI_MASTER      UINT32_C(65)

#define BVSTK_PL_CONTRACT_VERSION UINT32_C(1)

#endif /* BVSTK_AX7020_HW_CONFIG_H */
