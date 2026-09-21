#ifndef PL_SPI_TEST_CONFIG_H
#define PL_SPI_TEST_CONFIG_H

/* All settings belong to this FreeRTOS diagnostic, not the production driver. */
#define PL_SPI_TEST_RESET_MS          100U
#define PL_SPI_TEST_INIT_MS           5000U
#define PL_SPI_TEST_COMMAND_MS        3000U
#define PL_SPI_TEST_ERASE_MS          30000U
#define PL_SPI_TEST_RAW_MS            300U
#define PL_SPI_TEST_REPEATS           8U
#define PL_SPI_TEST_INIT_CLOCKS       100U
#define PL_SPI_TEST_INIT_DIV          512U
#define PL_SPI_TEST_DATA_DIV          512U
#define PL_SPI_TEST_READ_LBA          0U
/* FIFO_DEPTH in the pinned XSA; this IP parameter is not emitted into BSP. */
#define PL_SPI_TEST_TX_FIFO_WORDS     256U

/* Optional independent reference: CRC32/IEEE of the 512 bytes at READ_LBA. */
#define PL_SPI_TEST_HAVE_GOLDEN        0
#define PL_SPI_TEST_GOLDEN_CRC32       0x00000000U

/* Set these only for a dedicated card and an explicitly reserved LBA range.
 * The diagnostic deliberately does NOT restore overwritten card contents. */
#define PL_SPI_TEST_ENABLE_WRITE      0
#define PL_SPI_TEST_ENABLE_ERASE      0
#define PL_SPI_TEST_SCRATCH_LBA        0xFFFFFFFFU
#define PL_SPI_TEST_SCRATCH_SECTORS    0U
/* Select the card's specified erased byte (0x00 or 0xFF), not a guessed value. */
#define PL_SPI_TEST_ERASED_BYTE        (-1)

#if PL_SPI_TEST_ENABLE_WRITE && \
    (PL_SPI_TEST_SCRATCH_LBA == 0U || \
     PL_SPI_TEST_SCRATCH_LBA == 0xFFFFFFFFU || \
     PL_SPI_TEST_SCRATCH_SECTORS < 2U || PL_SPI_TEST_SCRATCH_SECTORS > 8U)
#error "Configure a reserved range of 2..8 sectors before enabling SD writes"
#endif
#if PL_SPI_TEST_ENABLE_WRITE && \
    (PL_SPI_TEST_SCRATCH_LBA > 0xFFFFFFFFU - PL_SPI_TEST_SCRATCH_SECTORS)
#error "Scratch range and its following guard sector must fit in a 32-bit LBA"
#endif
#if PL_SPI_TEST_ENABLE_ERASE && (!PL_SPI_TEST_ENABLE_WRITE || \
    (PL_SPI_TEST_ERASED_BYTE != 0 && PL_SPI_TEST_ERASED_BYTE != 255))
#error "Erase testing requires writes and the card's documented erased byte"
#endif
#if PL_SPI_TEST_REPEATS < 2U || PL_SPI_TEST_REPEATS > 1000U
#error "Select 2..1000 read repetitions"
#endif
#if PL_SPI_TEST_INIT_CLOCKS < 74U || PL_SPI_TEST_INIT_CLOCKS > 32767U
#error "SD initialization clock count is outside the supported field"
#endif
#if PL_SPI_TEST_INIT_DIV < 250U || PL_SPI_TEST_INIT_DIV > 1000U || \
    (PL_SPI_TEST_INIT_DIV & 1U) || PL_SPI_TEST_DATA_DIV < 2U || \
    PL_SPI_TEST_DATA_DIV > 65534U || (PL_SPI_TEST_DATA_DIV & 1U)
#error "Invalid SPI divider for the 100 MHz hardware profile"
#endif

#endif
