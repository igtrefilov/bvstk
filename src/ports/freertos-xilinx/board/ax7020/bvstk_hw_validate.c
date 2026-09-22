#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "xparameters.h"

/* SMI is the immutable IP instance name in the supplied XSA, not a PS API. */
#if BVSTK_PL_HAS_MDIO_CORE
#if !defined(XPAR_SMI_MASTER_0_BASEADDR) || !defined(XPAR_SMI_SLAVE_0_BASEADDR)
#error "PL contract mismatch: MDIO master/slave missing"
#elif XPAR_SMI_MASTER_0_BASEADDR != BVSTK_MDIO_MASTER_BASE || \
      XPAR_SMI_SLAVE_0_BASEADDR != BVSTK_MDIO_SLAVE_BASE
#error "PL contract mismatch: MDIO addresses"
#endif
#if XPAR_BRAM_1_BASEADDR != BVSTK_MDIO_BRAM_BASE || \
    XPAR_BRAM_1_HIGHADDR + 1U - XPAR_BRAM_1_BASEADDR != BVSTK_MDIO_BRAM_SIZE
#error "PL contract mismatch: MDIO BRAM"
#endif
#endif
#if BVSTK_PL_HAS_I2C_CORE
#if XPAR_I2C_MASTER_0_BASEADDR != BVSTK_I2C_MASTER_BASE || \
    XPAR_I2C_SLAVE_0_BASEADDR != BVSTK_I2C_SLAVE_BASE
#error "PL contract mismatch: I2C addresses"
#endif
#endif

/* Validate the generated address when the full block-I/O controller is used. */
#if BVSTK_PL_SD_CAN_BLOCK_IO
#if !defined(XPAR_SD_SPI_AXI_0_BASEADDR)
#error "PL contract mismatch: SD SPI AXI controller is missing"
#elif XPAR_SD_SPI_AXI_0_BASEADDR != BVSTK_SD_CONTROLLER_BASE
#error "PL contract mismatch: SD SPI controller base"
#endif

#if defined(XPAR_SD_SPI_AXI_0_HIGHADDR) && \
    (XPAR_SD_SPI_AXI_0_HIGHADDR - XPAR_SD_SPI_AXI_0_BASEADDR + 1U) != \
        BVSTK_SD_CONTROLLER_SIZE
#error "PL contract mismatch: SD SPI controller size"
#endif
#endif

/*
 * In init-only mode the generated hierarchical IP macro is intentionally not
 * part of the portable contract: its spelling differs between Vitis releases.
 * The address is fixed by BVSTK_SD_CONTROLLER_BASE and checked against the
 * develop XSA before the PS build.
 */
