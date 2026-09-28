#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "xqspips.h"
#include "xstatus.h"
#include "ports/freertos-xilinx/storage/qspi-flash/qspi_flash.h"

static u8 flash[QSPI_FLASH_SIZE_BYTES];
static int write_enabled;
static unsigned transfers;
static u8 capacity = 0x18;
static XQspiPs_Config cfg;

XQspiPs_Config *XQspiPs_LookupConfig(u16 id) { (void)id; return &cfg; }
int XQspiPs_CfgInitialize(XQspiPs *q, XQspiPs_Config *c, u32 b) { (void)q; (void)c; (void)b; return 0; }
int XQspiPs_SelfTest(XQspiPs *q) { (void)q; return 0; }
int XQspiPs_SetOptions(XQspiPs *q, u32 o) { (void)q; (void)o; return 0; }
void XQspiPs_SetClkPrescaler(XQspiPs *q, u8 p) { (void)q; (void)p; }
void XQspiPs_SetSlaveSelect(XQspiPs *q) { (void)q; }
int xil_printf(const char *format, ...) { (void)format; return 0; }

int XQspiPs_PolledTransfer(XQspiPs *q, u8 *tx, u8 *rx, u32 count)
{
    u32 address;
    (void)q;
    transfers++;
    if (rx) memset(rx, 0, count);
    if (tx[0] == 0x9F) {
        assert(count == 4 && rx);
        rx[1] = 0xEF; rx[2] = 0x40; rx[3] = capacity;
        return 0;
    }
    if (tx[0] == 0x06) { assert(count == 1); write_enabled = 1; return 0; }
    if (tx[0] == 0x05) { assert(count == 2 && rx); return 0; }
    assert(count >= 5);
    address = ((u32)tx[1] << 24) | ((u32)tx[2] << 16) | ((u32)tx[3] << 8) | tx[4];
    assert(address < sizeof(flash));
    switch (tx[0]) {
    case 0x13:
        assert(rx && count % 4 == 0); /* BSP RX-alignment requirement */
        for (u32 i = 5; i < count; ++i) rx[i] = flash[(address + i - 5) % sizeof(flash)];
        break;
    case 0x12:
        assert(write_enabled && !rx && count > 5);
        assert((address % 256) + count - 5 <= 256);
        for (u32 i = 5; i < count; ++i) flash[address + i - 5] &= tx[i];
        write_enabled = 0;
        break;
    case 0x21:
        assert(write_enabled && count == 5 && !rx && address % 4096 == 0);
        memset(flash + address, 0xFF, 4096);
        write_enabled = 0;
        break;
    default:
        assert(!"Unexpected opcode (legacy 24-bit addressing must not be used)");
    }
    return 0;
}

int main(void)
{
    u8 data[520], readback[520];
    unsigned previous;
    memset(flash, 0xFF, sizeof(flash));
    assert(qspi_flash_init() == XST_FAILURE); /* unsupported capacity */
    capacity = 0x19;
    assert(qspi_flash_init() == XST_SUCCESS);
    for (u32 i = 0; i < sizeof(data); ++i) data[i] = (u8)(i * 17 + 3);
    flash[0] = 0x5A;
    assert(qspi_flash_program(0x00FFFF00, data, sizeof(data)) == XST_SUCCESS);
    assert(qspi_flash_read(0x00FFFF00, readback, sizeof(readback)) == XST_SUCCESS);
    assert(memcmp(data, readback, sizeof(data)) == 0 && flash[0] == 0x5A);
    for (u32 length = 1; length <= 9; ++length) {
        assert(qspi_flash_read(0x01000000, readback, length) == XST_SUCCESS);
        assert(memcmp(readback, data + 256, length) == 0);
    }
    assert(qspi_flash_erase_sector(0x01000000) == XST_SUCCESS);
    assert(flash[0] == 0x5A && flash[0x01000000] == 0xFF);
    previous = transfers;
    assert(qspi_flash_program(0xFFFFFFFE, data, 4) == XST_FAILURE);
    assert(qspi_flash_read(QSPI_FLASH_SIZE_BYTES - 1, data, 2) == XST_FAILURE);
    assert(qspi_flash_erase_sector(QSPI_FLASH_SIZE_BYTES) == XST_FAILURE);
    assert(qspi_flash_erase_sector(1) == XST_FAILURE);
    assert(qspi_flash_read(0, data, 0) == XST_FAILURE);
    assert(transfers == previous);
    memset(flash + QSPI_FLASH_SIZE_BYTES - 4096, 0xA6, 4096);
    memset(flash + 0x01000000 - 4096, 0x3C, 4096);
    assert(qspi_flash_self_test() == XST_SUCCESS);
    for (u32 i = 0; i < 4096; ++i) {
        assert(flash[QSPI_FLASH_SIZE_BYTES - 4096 + i] == 0xA6);
        assert(flash[0x01000000 - 4096 + i] == 0x3C);
    }
    assert(flash[0] == 0x5A);
    puts("QSPI 32 MiB addressing, page boundaries, bounds and restore checks passed");
    return 0;
}
