#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "xparameters.h"

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
