#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "xparameters.h"

#if XPAR_I2C_MASTER_0_BASEADDR != BVSTK_I2C_MASTER_BASE
#error "PL contract mismatch: I2C master base"
#if XPAR_AXI_I2C_SLAVE_0_S00_AXI_BASEADDR != BVSTK_I2C_SLAVE_BASE
#error "PL contract mismatch: I2C slave base"
#endif
#if XPAR_SMI_MASTER_0_BASEADDR != BVSTK_SMI_MASTER_BASE
#error "PL contract mismatch: SMI master base"
#endif
#if XPAR_SMI_SLAVE_0_BASEADDR != BVSTK_SMI_SLAVE_BASE
#error "PL contract mismatch: SMI slave base"
#endif
#if XPAR_SPI_MASTER_0_BASEADDR != BVSTK_SPI_MASTER_BASE
#error "PL contract mismatch: SPI master base"
#endif

#if XPAR_BRAM_0_BASEADDR != BVSTK_I2C_BRAM_BASE
#error "PL contract mismatch: I2C BRAM base"
#endif
#if XPAR_BRAM_1_BASEADDR != BVSTK_SMI_BRAM_BASE
#error "PL contract mismatch: SMI BRAM base"
#endif
#if XPAR_BRAM_2_BASEADDR != BVSTK_SPI_BRAM_BASE
#error "PL contract mismatch: SPI BRAM base"
#endif

#if XPAR_FABRIC_SMI_MASTER_0_IRQ_INTR != BVSTK_IRQ_SMI_MASTER
#error "PL contract mismatch: SMI master IRQ"
#endif
#if XPAR_FABRIC_SMI_SLAVE_0_IRQ_INTR != BVSTK_IRQ_SMI_SLAVE
#error "PL contract mismatch: SMI slave IRQ"
#endif
#if XPAR_FABRIC_I2C_MASTER_0_IRQ_INTR != BVSTK_IRQ_I2C_MASTER
#error "PL contract mismatch: I2C master IRQ"
#endif
#if XPAR_FABRIC_AXI_I2C_SLAVE_0_IRQ_INTR != BVSTK_IRQ_I2C_SLAVE
#error "PL contract mismatch: I2C slave IRQ"
#endif
#if XPAR_FABRIC_SPI_MASTER_0_IRQ_INTR != BVSTK_IRQ_SPI_MASTER
#error "PL contract mismatch: SPI master IRQ"
#endif

#endif
