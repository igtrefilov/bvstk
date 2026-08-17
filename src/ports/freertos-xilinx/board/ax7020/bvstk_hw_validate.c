#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "xparameters.h"

/*
 * The playground XSA contains one custom PL peripheral.  Keep this check
 * deliberately tied to the generated IP name so stale legacy xparameters.h
 * files cannot make the application appear compatible with another design.
 */
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
