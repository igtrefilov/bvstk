/* AX7020 W25Q256 snapshot helper, executed in OCM through JTAG.
 * Only JEDEC ID (9F) and four-byte-address READ (13) are sent to Flash.
 * DDR and MIO must have been initialized using the matching ps7_init.tcl.
 */
#include "xparameters.h"
#include "xqspips.h"
#include "xil_cache.h"
#include "xil_io.h"
#include "xstatus.h"
#include <string.h>

#define REPORT_ADDR 0x07000000U
#define DATA_ADDR   0x08000000U
#define FLASH_SIZE  0x02000000U
#define CHUNK_SIZE  4096U
#define DONE       0x51444F4EU
#define ERROR      0x51455252U

static XQspiPs qspi;
/* The BSP's polled reader pads its first RX word for unknown opcodes when
 * ByteCount is not word-aligned. A 5-byte command plus three extra read bytes
 * keeps the transfer aligned without inserting a dummy cycle on the wire. */
static u8 tx[CHUNK_SIZE + 8U];
static u8 rx[CHUNK_SIZE + 8U];

int main(void)
{
    XQspiPs_Config *cfg;
    u32 offset, id = 0U, error = 1U;
    Xil_DCacheDisable();
    Xil_Out32(REPORT_ADDR, 0U);
    Xil_Out32(REPORT_ADDR + 4U, 0U);
    Xil_Out32(REPORT_ADDR + 8U, 0U);
    cfg = XQspiPs_LookupConfig(XPAR_XQSPIPS_0_DEVICE_ID);
    if (cfg == NULL || XQspiPs_CfgInitialize(&qspi, cfg, cfg->BaseAddress) != XST_SUCCESS)
        goto failed;
    error = 2U;
    if (XQspiPs_SetOptions(&qspi, XQSPIPS_FORCE_SSELECT_OPTION |
                         XQSPIPS_HOLD_B_DRIVE_OPTION) != XST_SUCCESS)
        goto failed;
    XQspiPs_SetClkPrescaler(&qspi, XQSPIPS_CLK_PRESCALE_8);
    XQspiPs_SetSlaveSelect(&qspi);
    tx[0] = 0x9FU;
    error = 3U;
    if (XQspiPs_PolledTransfer(&qspi, tx, rx, 4U) != XST_SUCCESS)
        goto failed;
    id = ((u32)rx[1] << 16) | ((u32)rx[2] << 8) | rx[3];
    Xil_Out32(REPORT_ADDR + 4U, id);
    error = 4U;
    /* The helper deliberately refuses other vendors/geometries. */
    if (rx[1] != 0xEFU || rx[3] != 0x19U)
        goto failed;
    memset(tx, 0, sizeof(tx));
    tx[0] = 0x13U;
    error = 5U;
    for (offset = 0U; offset < FLASH_SIZE; offset += CHUNK_SIZE) {
        tx[1] = (u8)(offset >> 24);
        tx[2] = (u8)(offset >> 16);
        tx[3] = (u8)(offset >> 8);
        tx[4] = (u8)offset;
        if (XQspiPs_PolledTransfer(&qspi, tx, rx, CHUNK_SIZE + 8U) != XST_SUCCESS)
            goto failed;
        memcpy((void *)(DATA_ADDR + offset), rx + 5, CHUNK_SIZE);
        Xil_Out32(REPORT_ADDR + 8U, offset + CHUNK_SIZE);
    }
    Xil_Out32(REPORT_ADDR, DONE);
    for (;;) { __asm__ volatile("wfi"); }
failed:
    Xil_Out32(REPORT_ADDR + 12U, error);
    Xil_Out32(REPORT_ADDR, ERROR);
    for (;;) { __asm__ volatile("wfi"); }
}
