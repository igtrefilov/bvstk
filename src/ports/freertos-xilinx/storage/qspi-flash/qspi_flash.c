#include "ports/freertos-xilinx/storage/qspi-flash/qspi_flash.h"
#include "xparameters.h"
#include "xqspips.h"
#include "xstatus.h"
#include "xil_printf.h"
#include "sleep.h"
#include <string.h>

#define QSPI_DEVICE_ID           XPAR_XQSPIPS_0_DEVICE_ID
#define QSPI_TEST_ADDR           (QSPI_FLASH_SIZE_BYTES - QSPI_FLASH_SECTOR_SIZE)
#define QSPI_STATUS_WIP_MASK     0x01U

/* W25Q256 dedicated four-byte opcodes address all 32 MiB without changing
 * the chip's address mode or bank register used by the Zynq BootROM/FSBL. */
#define CMD_WRITE_ENABLE         0x06U
#define CMD_READ_STATUS          0x05U
#define CMD_READ_ID              0x9FU
#define CMD_PAGE_PROGRAM         0x12U
#define CMD_READ_DATA            0x13U
#define CMD_SECTOR_ERASE_4K       0x21U
#define QSPI_ADDRESS_HEADER      5U

static XQspiPs qspi;
static int qspi_ready = 0;

static u8 tx_buf[8 + QSPI_FLASH_PAGE_SIZE];
static u8 tx_read_buf[8 + QSPI_FLASH_SECTOR_SIZE];
static u8 rx_buf[8 + QSPI_FLASH_SECTOR_SIZE];
static u8 sector_backup[QSPI_FLASH_SECTOR_SIZE];
static u8 test_pattern[QSPI_FLASH_PAGE_SIZE];
static u8 verify_buf[QSPI_FLASH_PAGE_SIZE];

static int qspi_flash_wait_ready(u32 timeout_ms);

static int qspi_flash_range_valid(u32 address, u32 length)
{
    return length != 0U && address < QSPI_FLASH_SIZE_BYTES &&
           length <= QSPI_FLASH_SIZE_BYTES - address;
}

static void qspi_flash_address(u8 *command, u8 opcode, u32 address)
{
    command[0] = opcode;
    command[1] = (u8)(address >> 24);
    command[2] = (u8)(address >> 16);
    command[3] = (u8)(address >> 8);
    command[4] = (u8)address;
}

int qspi_flash_init(void)
{
    if (qspi_ready) return XST_SUCCESS;

    XQspiPs_Config *cfg = XQspiPs_LookupConfig(QSPI_DEVICE_ID);
    if (!cfg) return XST_FAILURE;

    int status = XQspiPs_CfgInitialize(&qspi, cfg, cfg->BaseAddress);
    if (status != XST_SUCCESS) return status;

    status = XQspiPs_SelfTest(&qspi);
    if (status != XST_SUCCESS) return status;

    /* Keep chip select asserted across transfers and use safe clock. */
    u32 options = XQSPIPS_FORCE_SSELECT_OPTION | XQSPIPS_HOLD_B_DRIVE_OPTION;
    status = XQspiPs_SetOptions(&qspi, options);
    if (status != XST_SUCCESS) return status;

    XQspiPs_SetClkPrescaler(&qspi, XQSPIPS_CLK_PRESCALE_8);
    XQspiPs_SetSlaveSelect(&qspi); /* single slave wired to SS0 */

    memset(tx_buf, 0, 4U);
    tx_buf[0] = CMD_READ_ID;
    status = XQspiPs_PolledTransfer(&qspi, tx_buf, rx_buf, 4U);
    if (status != XST_SUCCESS) return status;
    if (rx_buf[1] != 0xEFU || rx_buf[3] != 0x19U) {
        xil_printf("QSPI: unsupported JEDEC ID %02x%02x%02x (expected W25Q256)\r\n",
                   rx_buf[1], rx_buf[2], rx_buf[3]);
        return XST_FAILURE;
    }

    qspi_ready = 1;
    return XST_SUCCESS;
}

static int qspi_flash_write_enable(void)
{
    tx_buf[0] = CMD_WRITE_ENABLE;
    return XQspiPs_PolledTransfer(&qspi, tx_buf, NULL, 1);
}

static int qspi_flash_read_status(u8 *status)
{
    if (!status) return XST_FAILURE;
    tx_buf[0] = CMD_READ_STATUS;
    tx_buf[1] = 0;
    int rc = XQspiPs_PolledTransfer(&qspi, tx_buf, rx_buf, 2);
    if (rc != XST_SUCCESS) return rc;
    *status = rx_buf[1];
    return XST_SUCCESS;
}

static int qspi_flash_wait_ready(u32 timeout_ms)
{
    u8 status = 0;
    for (u32 i = 0; i < timeout_ms; ++i) {
        int rc = qspi_flash_read_status(&status);
        if (rc != XST_SUCCESS) return rc;
        if ((status & QSPI_STATUS_WIP_MASK) == 0) return XST_SUCCESS;
        usleep(1000);
    }
    return XST_FAILURE;
}

int qspi_flash_erase_sector(u32 address)
{
    if (!qspi_ready || address % QSPI_FLASH_SECTOR_SIZE != 0U ||
        !qspi_flash_range_valid(address, QSPI_FLASH_SECTOR_SIZE)) return XST_FAILURE;
    int rc = qspi_flash_write_enable();
    if (rc != XST_SUCCESS) return rc;

    qspi_flash_address(tx_buf, CMD_SECTOR_ERASE_4K, address);

    rc = XQspiPs_PolledTransfer(&qspi, tx_buf, NULL, QSPI_ADDRESS_HEADER);
    if (rc != XST_SUCCESS) return rc;

    return qspi_flash_wait_ready(5000);
}

int qspi_flash_program(u32 address, const u8 *data, u32 length)
{
    if (!qspi_ready || !data || !qspi_flash_range_valid(address, length)) return XST_FAILURE;

    u32 offset = 0;
    while (offset < length) {
        u32 page_off = (address + offset) % QSPI_FLASH_PAGE_SIZE;
        u32 chunk = QSPI_FLASH_PAGE_SIZE - page_off;
        if (chunk > (length - offset)) chunk = length - offset;

        int rc = qspi_flash_write_enable();
        if (rc != XST_SUCCESS) return rc;

        u32 addr = address + offset;
        qspi_flash_address(tx_buf, CMD_PAGE_PROGRAM, addr);
        memcpy(&tx_buf[QSPI_ADDRESS_HEADER], &data[offset], chunk);

        rc = XQspiPs_PolledTransfer(&qspi, tx_buf, NULL, QSPI_ADDRESS_HEADER + chunk);
        if (rc != XST_SUCCESS) return rc;

        rc = qspi_flash_wait_ready(5000);
        if (rc != XST_SUCCESS) return rc;

        offset += chunk;
    }

    return XST_SUCCESS;
}

int qspi_flash_read(u32 address, u8 *data, u32 length)
{
    if (!qspi_ready || !data || !qspi_flash_range_valid(address, length)) return XST_FAILURE;

    const u32 max_chunk = QSPI_FLASH_SECTOR_SIZE;
    u32 offset = 0;
    while (offset < length) {
        u32 chunk = (length - offset > max_chunk) ? max_chunk : (length - offset);
        u32 addr = address + offset;

        /* For opcodes absent from the BSP's instruction table, its polled
         * RX path pads the first word if ByteCount is not a multiple of 4.
         * Clock up to three extra bytes and keep only the requested data. */
        u32 transfer_size = (QSPI_ADDRESS_HEADER + chunk + 3U) & ~3U;
        qspi_flash_address(tx_read_buf, CMD_READ_DATA, addr);
        memset(&tx_read_buf[QSPI_ADDRESS_HEADER], 0,
               transfer_size - QSPI_ADDRESS_HEADER);

        int rc = XQspiPs_PolledTransfer(&qspi, tx_read_buf, rx_buf, transfer_size);
        if (rc != XST_SUCCESS) return rc;

        memcpy(&data[offset], &rx_buf[QSPI_ADDRESS_HEADER], chunk);
        offset += chunk;
    }

    return XST_SUCCESS;
}

static void fill_test_pattern(u8 *buf, u32 length)
{
    for (u32 i = 0; i < length; ++i) {
        buf[i] = (u8)((i * 17U) ^ 0xA5U);
    }
}

int qspi_flash_self_test(void)
{
    int rc = qspi_flash_init();
    if (rc != XST_SUCCESS) {
        xil_printf("QSPI: init failed (%d)\r\n", rc);
        return rc;
    }

    xil_printf("QSPI: starting self-test at 0x%08x\r\n", QSPI_TEST_ADDR);

    rc = qspi_flash_read(QSPI_TEST_ADDR, sector_backup, sizeof(sector_backup));
    if (rc != XST_SUCCESS) {
        xil_printf("QSPI: read backup failed (%d)\r\n", rc);
        return rc;
    }

    fill_test_pattern(test_pattern, sizeof(test_pattern));

    rc = qspi_flash_erase_sector(QSPI_TEST_ADDR);
    if (rc != XST_SUCCESS) {
        xil_printf("QSPI: erase failed (%d)\r\n", rc);
        return rc;
    }

    rc = qspi_flash_program(QSPI_TEST_ADDR, test_pattern, sizeof(test_pattern));
    if (rc != XST_SUCCESS) {
        xil_printf("QSPI: program failed (%d)\r\n", rc);
        goto restore;
    }

    rc = qspi_flash_read(QSPI_TEST_ADDR, verify_buf, sizeof(verify_buf));
    if (rc != XST_SUCCESS) {
        xil_printf("QSPI: read verify failed (%d)\r\n", rc);
        goto restore;
    }

    if (memcmp(test_pattern, verify_buf, sizeof(test_pattern)) != 0) {
        xil_printf("QSPI: self-test FAILED (data mismatch)\r\n");
        rc = XST_FAILURE;
    } else {
        xil_printf("QSPI: self-test PASSED\r\n");
    }

restore:
    /* Try to restore previous contents to avoid breaking boot images. */
    {
        int restore_rc = qspi_flash_erase_sector(QSPI_TEST_ADDR);
        if (restore_rc == XST_SUCCESS) {
            restore_rc = qspi_flash_program(QSPI_TEST_ADDR, sector_backup, sizeof(sector_backup));
        }
        if (restore_rc != XST_SUCCESS) {
            xil_printf("QSPI: restore failed (%d)\r\n", restore_rc);
        }
    }

    return rc;
}
