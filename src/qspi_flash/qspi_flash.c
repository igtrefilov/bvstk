#include "qspi_flash.h"
#include "xparameters.h"
#include "xqspips.h"
#include "xstatus.h"
#include "xil_printf.h"
#include "sleep.h"
#include <string.h>

#define QSPI_DEVICE_ID           XPAR_XQSPIPS_0_DEVICE_ID
#define QSPI_FLASH_PAGE_SIZE     256U
#define QSPI_FLASH_SECTOR_SIZE   0x1000U   /* 4 KB */
#define QSPI_FLASH_SIZE_BYTES    (32U * 1024U * 1024U) /* 256 Mbit */
#define QSPI_TEST_ADDR           (QSPI_FLASH_SIZE_BYTES - QSPI_FLASH_SECTOR_SIZE)
#define QSPI_STATUS_WIP_MASK     0x01U

/* Flash opcodes for Winbond W25Q family (JEDEC common). */
#define CMD_WRITE_ENABLE         0x06U
#define CMD_READ_STATUS          0x05U
#define CMD_PAGE_PROGRAM         0x02U
#define CMD_READ_DATA            0x03U
#define CMD_SECTOR_ERASE_4K      0x20U

static XQspiPs qspi;
static int qspi_ready = 0;

static u8 tx_buf[4 + QSPI_FLASH_PAGE_SIZE];
static u8 tx_read_buf[4 + QSPI_FLASH_SECTOR_SIZE];
static u8 rx_buf[4 + QSPI_FLASH_SECTOR_SIZE];
static u8 sector_backup[QSPI_FLASH_SECTOR_SIZE];
static u8 test_pattern[QSPI_FLASH_PAGE_SIZE];
static u8 verify_buf[QSPI_FLASH_PAGE_SIZE];

static int qspi_flash_wait_ready(u32 timeout_ms);

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
    if (!qspi_ready) return XST_FAILURE;
    int rc = qspi_flash_write_enable();
    if (rc != XST_SUCCESS) return rc;

    tx_buf[0] = CMD_SECTOR_ERASE_4K;
    tx_buf[1] = (address >> 16) & 0xFF;
    tx_buf[2] = (address >> 8) & 0xFF;
    tx_buf[3] = address & 0xFF;

    rc = XQspiPs_PolledTransfer(&qspi, tx_buf, NULL, 4);
    if (rc != XST_SUCCESS) return rc;

    return qspi_flash_wait_ready(5000);
}

int qspi_flash_program(u32 address, const u8 *data, u32 length)
{
    if (!qspi_ready || !data || length == 0) return XST_FAILURE;

    u32 offset = 0;
    while (offset < length) {
        u32 page_off = (address + offset) % QSPI_FLASH_PAGE_SIZE;
        u32 chunk = QSPI_FLASH_PAGE_SIZE - page_off;
        if (chunk > (length - offset)) chunk = length - offset;

        int rc = qspi_flash_write_enable();
        if (rc != XST_SUCCESS) return rc;

        tx_buf[0] = CMD_PAGE_PROGRAM;
        u32 addr = address + offset;
        tx_buf[1] = (addr >> 16) & 0xFF;
        tx_buf[2] = (addr >> 8) & 0xFF;
        tx_buf[3] = addr & 0xFF;
        memcpy(&tx_buf[4], &data[offset], chunk);

        rc = XQspiPs_PolledTransfer(&qspi, tx_buf, NULL, 4 + chunk);
        if (rc != XST_SUCCESS) return rc;

        rc = qspi_flash_wait_ready(5000);
        if (rc != XST_SUCCESS) return rc;

        offset += chunk;
    }

    return XST_SUCCESS;
}

int qspi_flash_read(u32 address, u8 *data, u32 length)
{
    if (!qspi_ready || !data || length == 0) return XST_FAILURE;

    const u32 max_chunk = QSPI_FLASH_SECTOR_SIZE;
    u32 offset = 0;
    while (offset < length) {
        u32 chunk = (length - offset > max_chunk) ? max_chunk : (length - offset);
        u32 addr = address + offset;

        tx_read_buf[0] = CMD_READ_DATA;
        tx_read_buf[1] = (addr >> 16) & 0xFF;
        tx_read_buf[2] = (addr >> 8) & 0xFF;
        tx_read_buf[3] = addr & 0xFF;
        memset(&tx_read_buf[4], 0, chunk);

        int rc = XQspiPs_PolledTransfer(&qspi, tx_read_buf, rx_buf, 4 + chunk);
        if (rc != XST_SUCCESS) return rc;

        memcpy(&data[offset], &rx_buf[4], chunk);
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
    int restore_rc = qspi_flash_erase_sector(QSPI_TEST_ADDR);
    if (restore_rc == XST_SUCCESS) {
        restore_rc = qspi_flash_program(QSPI_TEST_ADDR, sector_backup, sizeof(sector_backup));
    }
    if (restore_rc != XST_SUCCESS) {
        xil_printf("QSPI: restore failed (%d)\r\n", restore_rc);
    }

    return rc;
}
