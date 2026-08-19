/* FatFs disk I/O glue for the AX7020 FreeRTOS target. */

#include "diskio.h"
#include "ff.h"

#include <stdint.h>
#include <string.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "hardware/boards/ax7020/bvstk_qspi_layout.h"
#include "ports/freertos-xilinx/storage/qspi-flash/qspi_flash.h"
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.h"
#include "xil_types.h"
#include "xparameters.h"
#include "xstatus.h"

#ifdef XPAR_XSDPS_NUM_INSTANCES
#include "sleep.h"
#include "xsdps.h"
#endif

typedef DWORD LBA_t;

#ifndef CTRL_TRIM
#define CTRL_TRIM CTRL_ERASE_SECTOR
#endif

enum {
    DISKIO_PS_SD_PDRV = 0,
    DISKIO_QSPI_PDRV = 1,
    DISKIO_PL_SD_PDRV = 2,
    DISKIO_STAT_COUNT = 3,
    DISKIO_SECTOR_SIZE = BVSTK_SD_SECTOR_SIZE,
    DISKIO_SD_ERASE_BLOCK_SECTORS = 128U
};

static DSTATUS s_status[DISKIO_STAT_COUNT] = {
    STA_NOINIT,
    STA_NOINIT,
    STA_NOINIT
};

static int s_qspi_ready;
static uint8_t s_qspi_sector_cache[QSPI_FLASH_SECTOR_SIZE];

#ifdef XPAR_XSDPS_NUM_INSTANCES
static XSdPs s_ps_sd;
static UINTPTR s_ps_sd_base;
static u32 s_ps_sd_card_detect;
static u32 s_ps_sd_write_protect;
static u32 s_ps_sd_slot_type;
static u8 s_ps_sd_host_controller_version;
static int s_ps_sd_configured;
static int s_ps_sd_ready;
#endif

static int valid_pdrv(BYTE pdrv)
{
    return pdrv < (BYTE)DISKIO_STAT_COUNT;
}

static int lba_range_valid(LBA_t sector, UINT count)
{
    return count != 0U &&
           sector <= (LBA_t)UINT32_MAX &&
           (uint64_t)count <=
               (uint64_t)UINT32_MAX - (uint32_t)sector + UINT64_C(1);
}

static int qspi_lba_to_flash_addr(u32 *out_addr, LBA_t sector, u32 count)
{
    uint64_t byte_addr;
    uint64_t byte_count;
    uint64_t end_addr;

    if (out_addr == NULL || count == 0U) {
        return 0;
    }
    byte_addr = (uint64_t)sector * DISKIO_SECTOR_SIZE;
    byte_count = (uint64_t)count * DISKIO_SECTOR_SIZE;
    end_addr = byte_addr + byte_count;
    if (byte_addr > QSPI_FS_SIZE_BYTES ||
        byte_count > QSPI_FS_SIZE_BYTES ||
        end_addr > QSPI_FS_SIZE_BYTES ||
        end_addr < byte_addr) {
        return 0;
    }
    *out_addr = (u32)(QSPI_FS_BASE_BYTES + byte_addr);
    return 1;
}

static DRESULT qspi_write_sector(LBA_t sector, const BYTE *buffer)
{
    u32 address;
    u32 block_address;
    u32 offset;

    if (buffer == NULL || !qspi_lba_to_flash_addr(&address, sector, 1U)) {
        return RES_PARERR;
    }
    block_address = address & ~(QSPI_FLASH_SECTOR_SIZE - 1U);
    offset = address - block_address;
    if (qspi_flash_read(block_address,
                        s_qspi_sector_cache,
                        QSPI_FLASH_SECTOR_SIZE) != XST_SUCCESS) {
        return RES_ERROR;
    }
    memcpy(s_qspi_sector_cache + offset, buffer, DISKIO_SECTOR_SIZE);
    if (qspi_flash_erase_sector(block_address) != XST_SUCCESS ||
        qspi_flash_program(block_address,
                           s_qspi_sector_cache,
                           QSPI_FLASH_SECTOR_SIZE) != XST_SUCCESS) {
        return RES_ERROR;
    }
    return RES_OK;
}

#ifdef XPAR_XSDPS_NUM_INSTANCES
static int ps_sd_load_config(void)
{
    XSdPs_Config *config;

    if (s_ps_sd_configured) {
        return XST_SUCCESS;
    }
#ifndef SDT
    config = XSdPs_LookupConfig(0U);
#else
    config = XSdPs_LookupConfig(XSdPs_ConfigTable[0].BaseAddress);
#endif
    if (config == NULL) {
        return XST_FAILURE;
    }

    s_ps_sd_base = config->BaseAddress;
    s_ps_sd_card_detect = config->CardDetect;
    s_ps_sd_write_protect = config->WriteProtect;
    s_ps_sd_host_controller_version =
        (u8)(XSdPs_ReadReg16(s_ps_sd_base, XSDPS_HOST_CTRL_VER_OFFSET) &
             XSDPS_HC_SPEC_VER_MASK);
    if (s_ps_sd_host_controller_version == XSDPS_HC_SPEC_V3) {
        s_ps_sd_slot_type = XSdPs_ReadReg(s_ps_sd_base, XSDPS_CAPS_OFFSET) &
                            XSDPS_CAPS_SLOT_TYPE_MASK;
    } else {
        s_ps_sd_slot_type = 0U;
    }
    s_ps_sd_configured = 1;
    return XST_SUCCESS;
}

static DSTATUS ps_sd_status(void)
{
    DSTATUS status = s_status[DISKIO_PS_SD_PDRV];
    u32 present_status;
    u32 delay_count = 0U;

    if (ps_sd_load_config() != XST_SUCCESS) {
        return STA_NOINIT;
    }
    if (!s_ps_sd_ready) {
        return (DSTATUS)(status | STA_NOINIT);
    }
    if ((XSdPs_ReadReg8(s_ps_sd_base, XSDPS_POWER_CTRL_OFFSET) &
         XSDPS_PC_BUS_PWR_MASK) == 0U) {
        status |= STA_NOINIT;
    }

    present_status = XSdPs_GetPresentStatusReg(s_ps_sd_base);
    if (s_ps_sd_slot_type != XSDPS_CAPS_EMB_SLOT) {
        if (s_ps_sd_card_detect) {
            while ((present_status & XSDPS_PSR_CARD_INSRT_MASK) == 0U) {
                if (delay_count == 500U) {
                    status = STA_NODISK | STA_NOINIT;
                    break;
                }
                usleep(10000U);
                delay_count++;
                present_status = XSdPs_GetPresentStatusReg(s_ps_sd_base);
            }
        }
        if ((status & STA_NODISK) == 0U) {
            status &= (DSTATUS)~STA_NODISK;
        }
        if (s_ps_sd_write_protect &&
            (present_status & XSDPS_PSR_WPS_PL_MASK) == 0U) {
            status |= STA_PROTECT;
        } else {
            status &= (DSTATUS)~STA_PROTECT;
        }
    } else {
        status &= (DSTATUS)~(STA_NODISK | STA_PROTECT);
    }
    s_status[DISKIO_PS_SD_PDRV] = status;
    return status;
}

static DSTATUS ps_sd_initialize(void)
{
    XSdPs_Config *config;
    int result;

    if (ps_sd_load_config() != XST_SUCCESS) {
        return STA_NOINIT;
    }
    if (s_ps_sd_ready) {
        return ps_sd_status();
    }
    if (s_ps_sd_card_detect) {
        u32 present_status = XSdPs_GetPresentStatusReg(s_ps_sd_base);
        while ((present_status & (XSDPS_PSR_CARD_DPL_MASK |
                                  XSDPS_PSR_CARD_STABLE_MASK |
                                  XSDPS_PSR_CARD_INSRT_MASK)) !=
               (XSDPS_PSR_CARD_DPL_MASK |
                XSDPS_PSR_CARD_STABLE_MASK |
                XSDPS_PSR_CARD_INSRT_MASK)) {
            present_status = XSdPs_GetPresentStatusReg(s_ps_sd_base);
        }
    }

#ifndef SDT
    config = XSdPs_LookupConfig(0U);
#else
    config = XSdPs_LookupConfig(XSdPs_ConfigTable[0].BaseAddress);
#endif
    if (config == NULL) {
        s_status[DISKIO_PS_SD_PDRV] = STA_NOINIT;
        return s_status[DISKIO_PS_SD_PDRV];
    }

    s_ps_sd.IsReady = 0U;
    result = XSdPs_CfgInitialize(&s_ps_sd, config, config->BaseAddress);
    if (result == XST_SUCCESS) {
        result = XSdPs_CardInitialize(&s_ps_sd);
    }
    if (result != XST_SUCCESS) {
        s_ps_sd_ready = 0;
        s_status[DISKIO_PS_SD_PDRV] = STA_NOINIT;
        return s_status[DISKIO_PS_SD_PDRV];
    }
    s_ps_sd_ready = 1;
    s_status[DISKIO_PS_SD_PDRV] = 0U;
    return ps_sd_status();
}
#else
static DSTATUS ps_sd_status(void)
{
    return STA_NOINIT;
}

static DSTATUS ps_sd_initialize(void)
{
    return STA_NOINIT;
}
#endif

DSTATUS disk_status(BYTE pdrv)
{
    if (!valid_pdrv(pdrv)) {
        return STA_NOINIT;
    }
    switch (pdrv) {
    case DISKIO_PS_SD_PDRV:
        return ps_sd_status();
    case DISKIO_QSPI_PDRV:
        s_status[pdrv] = s_qspi_ready ? 0U : STA_NOINIT;
        return s_status[pdrv];
    case DISKIO_PL_SD_PDRV:
        s_status[pdrv] = bvstk_sd_pl_is_ready() ? 0U : STA_NOINIT;
        return s_status[pdrv];
    default:
        return STA_NOINIT;
    }
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (!valid_pdrv(pdrv)) {
        return STA_NOINIT;
    }
    if ((disk_status(pdrv) & STA_NODISK) != 0U) {
        return disk_status(pdrv);
    }
    if ((disk_status(pdrv) & STA_NOINIT) == 0U) {
        return disk_status(pdrv);
    }

    switch (pdrv) {
    case DISKIO_PS_SD_PDRV:
        return ps_sd_initialize();

    case DISKIO_QSPI_PDRV:
        if (qspi_flash_init() != XST_SUCCESS) {
            s_status[pdrv] = STA_NOINIT;
        } else {
            s_qspi_ready = 1;
            s_status[pdrv] = 0U;
        }
        return s_status[pdrv];

    case DISKIO_PL_SD_PDRV:
        s_status[pdrv] = bvstk_sd_pl_initialize() == BVSTK_OK
                             ? 0U
                             : STA_NOINIT;
        return s_status[pdrv];

    default:
        return STA_NOINIT;
    }
}

DRESULT disk_read(BYTE pdrv, BYTE *buffer, LBA_t sector, UINT count)
{
    DSTATUS status;

    if (!valid_pdrv(pdrv) || buffer == NULL || count == 0U) {
        return RES_PARERR;
    }
    status = disk_status(pdrv);
    if ((status & STA_NOINIT) != 0U) {
        return RES_NOTRDY;
    }
    if (pdrv == DISKIO_PS_SD_PDRV) {
#ifdef XPAR_XSDPS_NUM_INSTANCES
        DWORD location = (DWORD)sector;
        if (s_ps_sd.HCS == 0U) {
            location *= (DWORD)XSDPS_BLK_SIZE_512_MASK;
        }
        return XSdPs_ReadPolled(&s_ps_sd,
                                (u32)location,
                                count,
                                buffer) == XST_SUCCESS
                   ? RES_OK
                   : RES_ERROR;
#else
        return RES_NOTRDY;
#endif
    }
    if (pdrv == DISKIO_QSPI_PDRV) {
        u32 address;
        uint64_t bytes = (uint64_t)count * DISKIO_SECTOR_SIZE;
        if (bytes > UINT32_MAX || !qspi_lba_to_flash_addr(&address, sector, count)) {
            return RES_PARERR;
        }
        return qspi_flash_read(address, buffer, (u32)bytes) == XST_SUCCESS
                   ? RES_OK
                   : RES_ERROR;
    }
    if (!lba_range_valid(sector, count)) {
        return RES_PARERR;
    }
    return bvstk_sd_pl_read((uint32_t)sector,
                            buffer,
                            (size_t)count) == BVSTK_OK
               ? RES_OK
               : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE command, void *buffer)
{
    /* FatFs specifies a NULL argument for CTRL_SYNC. */
    if (!valid_pdrv(pdrv) ||
        (buffer == NULL && command != CTRL_SYNC)) {
        return RES_PARERR;
    }
    if ((disk_status(pdrv) & STA_NOINIT) != 0U) {
        return RES_NOTRDY;
    }

    switch (command) {
    case CTRL_SYNC:
        return RES_OK;

    case GET_SECTOR_COUNT:
        if (pdrv == DISKIO_PS_SD_PDRV) {
#ifdef XPAR_XSDPS_NUM_INSTANCES
            *(DWORD *)buffer = (DWORD)s_ps_sd.SectorCount;
            return RES_OK;
#else
            return RES_NOTRDY;
#endif
        }
        if (pdrv == DISKIO_QSPI_PDRV) {
            *(DWORD *)buffer = QSPI_FS_SIZE_BYTES / DISKIO_SECTOR_SIZE;
            return RES_OK;
        }
        return bvstk_sd_pl_get_sector_count((uint32_t *)buffer) == BVSTK_OK
                   ? RES_OK
                   : RES_PARERR;

    case GET_SECTOR_SIZE:
        *(DWORD *)buffer = DISKIO_SECTOR_SIZE;
        return RES_OK;

    case GET_BLOCK_SIZE:
        *(DWORD *)buffer = pdrv == DISKIO_QSPI_PDRV
                               ? QSPI_FLASH_SECTOR_SIZE / DISKIO_SECTOR_SIZE
                               : DISKIO_SD_ERASE_BLOCK_SECTORS;
        return RES_OK;

    case CTRL_TRIM:
        return RES_OK;

    default:
        return RES_PARERR;
    }
}

#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv,
                   const BYTE *buffer,
                   LBA_t sector,
                   UINT count)
{
    DSTATUS status;

    if (!valid_pdrv(pdrv) || buffer == NULL || count == 0U) {
        return RES_PARERR;
    }
    status = disk_status(pdrv);
    if ((status & STA_NOINIT) != 0U) {
        return RES_NOTRDY;
    }
    if (pdrv == DISKIO_PS_SD_PDRV) {
#ifdef XPAR_XSDPS_NUM_INSTANCES
        DWORD location = (DWORD)sector;
        if (s_ps_sd.HCS == 0U) {
            location *= (DWORD)XSDPS_BLK_SIZE_512_MASK;
        }
        return XSdPs_WritePolled(&s_ps_sd,
                                 (u32)location,
                                 count,
                                 buffer) == XST_SUCCESS
                   ? RES_OK
                   : RES_ERROR;
#else
        return RES_NOTRDY;
#endif
    }
    if (pdrv == DISKIO_QSPI_PDRV) {
        UINT index;
        for (index = 0U; index < count; ++index) {
            DRESULT result = qspi_write_sector(
                sector + index,
                buffer + index * DISKIO_SECTOR_SIZE);
            if (result != RES_OK) {
                return result;
            }
        }
        return RES_OK;
    }
    if (!lba_range_valid(sector, count)) {
        return RES_PARERR;
    }
    return bvstk_sd_pl_write((uint32_t)sector,
                             buffer,
                             (size_t)count) == BVSTK_OK
               ? RES_OK
               : RES_ERROR;
}
#endif

DWORD get_fattime(void)
{
    return ((DWORD)(2010U - 1980U) << 25U) |
           ((DWORD)1U << 21U) |
           ((DWORD)1U << 16U);
}
