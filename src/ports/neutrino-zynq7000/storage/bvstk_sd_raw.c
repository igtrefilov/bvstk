/*
 * AX7020 PS SD raw backend for Neutrino.
 *
 * The generic devb-sdmmc/SDHCI combination shipped with the current
 * Neutrino BSP does not reset the Zynq-7000 SD controller correctly: it
 * writes the command/data reset bits as a 32-bit SYSCTL value and leaves
 * them asserted.  The Xilinx PS SD driver used by the FreeRTOS target
 * writes the byte-wide SW_RST register and consequently works on the same
 * board and card.
 *
 * This small backend follows the Zynq SDHCI register protocol directly.  It
 * deliberately uses polled PIO transfers first.  It exposes a seekable raw
 * sector device; devb-loopback supplies the normal Neutrino block layer and
 * fs-dos mounts the existing FAT volume without changing its layout.
 */

#define _FILE_OFFSET_BITS 64

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/iofunc.h>
#include <sys/iomsg.h>
#include <sys/dispatch.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <sys/resmgr.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define SD_BASE                         ((uintptr_t)0xE0100000U)
#define SD_MAP_SIZE                     ((size_t)0x1000U)
#define SD_INPUT_CLOCK_HZ               UINT32_C(100000000)
#define SD_SECTOR_SIZE                  512U
#define SD_MAX_TRANSFER_BYTES           (64U * 1024U)
#define SD_TIMEOUT_US                   UINT32_C(2000000)
#define SD_INIT_TIMEOUT_US              UINT32_C(500000)

#define SD_SDMA_SYS_ADDR                0x00U
#define SD_BLK_SIZE                     0x04U
#define SD_BLK_COUNT                    0x06U
#define SD_ARGUMENT                     0x08U
#define SD_XFER_MODE                    0x0CU
#define SD_COMMAND                      0x0EU
#define SD_RESPONSE0                    0x10U
#define SD_RESPONSE1                    0x14U
#define SD_RESPONSE2                    0x18U
#define SD_RESPONSE3                    0x1CU
#define SD_BUFFER                       0x20U
#define SD_PRESENT_STATE                0x24U
#define SD_HOST_CONTROL1               0x28U
#define SD_POWER_CONTROL               0x29U
#define SD_CLOCK_CONTROL               0x2CU
#define SD_TIMEOUT_CONTROL             0x2EU
#define SD_SOFTWARE_RESET              0x2FU
#define SD_NORMAL_STATUS               0x30U
#define SD_ERROR_STATUS                0x32U
#define SD_NORMAL_STATUS_ENABLE        0x34U
#define SD_ERROR_STATUS_ENABLE         0x36U
#define SD_NORMAL_SIGNAL_ENABLE       0x38U
#define SD_ERROR_SIGNAL_ENABLE        0x3AU
#define SD_HOST_CONTROL2               0x3EU
#define SD_CAPABILITIES                0x40U
#define SD_HOST_CONTROL_VERSION        0xFEU

#define SD_HOST_WIDTH_4                UINT8_C(0x02)
#define SD_POWER_3V3                   UINT8_C(0x0F)

#define SD_CLOCK_INTERNAL_ENABLE       UINT16_C(0x0001)
#define SD_CLOCK_INTERNAL_STABLE       UINT16_C(0x0002)
#define SD_CLOCK_SD_ENABLE             UINT16_C(0x0004)
#define SD_CLOCK_DIV_256               UINT16_C(0x8000)
#define SD_CLOCK_DIV_4                 UINT16_C(0x0200)

#define SD_RESET_ALL                    UINT8_C(0x01)
#define SD_RESET_CMD                    UINT8_C(0x02)
#define SD_RESET_DATA                   UINT8_C(0x04)

#define SD_STATUS_COMMAND_COMPLETE      UINT16_C(0x0001)
#define SD_STATUS_TRANSFER_COMPLETE     UINT16_C(0x0002)
#define SD_STATUS_BUFFER_WRITE_READY    UINT16_C(0x0010)
#define SD_STATUS_BUFFER_READ_READY     UINT16_C(0x0020)
#define SD_STATUS_ERROR                 UINT16_C(0x8000)
#define SD_STATUS_ALL                   UINT16_C(0xFFFF)
#define SD_ERROR_ALL                    UINT16_C(0xF3FF)

#define SD_PRESENT_CMD_INHIBIT          UINT32_C(0x00000001)
#define SD_PRESENT_DATA_INHIBIT         UINT32_C(0x00000002)
#define SD_PRESENT_BUFFER_WRITE         UINT32_C(0x00000400)
#define SD_PRESENT_BUFFER_READ          UINT32_C(0x00000800)
#define SD_PRESENT_CARD_INSERTED       UINT32_C(0x00010000)

#define SD_XFER_BLOCK_COUNT_ENABLE      UINT16_C(0x0002)
#define SD_XFER_READ                    UINT16_C(0x0010)
#define SD_XFER_MULTI                   UINT16_C(0x0020)

#define SD_CMD_RESPONSE_NONE            UINT16_C(0x0000)
#define SD_CMD_RESPONSE_136             UINT16_C(0x0001)
#define SD_CMD_RESPONSE_48              UINT16_C(0x0002)
#define SD_CMD_RESPONSE_48_BUSY         UINT16_C(0x0003)
#define SD_CMD_CRC_CHECK                UINT16_C(0x0008)
#define SD_CMD_INDEX_CHECK              UINT16_C(0x0010)
#define SD_CMD_DATA_PRESENT             UINT16_C(0x0020)

#define SD_CMD0                         UINT8_C(0)
#define SD_CMD2                         UINT8_C(2)
#define SD_CMD3                         UINT8_C(3)
#define SD_CMD7                         UINT8_C(7)
#define SD_CMD8                         UINT8_C(8)
#define SD_CMD9                         UINT8_C(9)
#define SD_CMD16                        UINT8_C(16)
#define SD_CMD17                        UINT8_C(17)
#define SD_CMD24                        UINT8_C(24)
#define SD_CMD41                        UINT8_C(41)
#define SD_CMD55                        UINT8_C(55)

#define SD_ACMD41                       UINT8_C(41)
#define SD_ACMD6                        UINT8_C(6)

#define SD_OCR_POWER_UP                 UINT32_C(0x80000000)
#define SD_OCR_HIGH_CAPACITY            UINT32_C(0x40000000)
#define SD_OCR_3V3                      UINT32_C(0x00FF8000)
#define SD_CMD8_ARGUMENT                UINT32_C(0x000001AA)

#define SD_CARD_STATE_TRAN              UINT32_C(4)
#define SD_CARD_STATUS_CURRENT_STATE    UINT32_C(0x00001E00)

#define SD_IO_CHUNK                     (8U * SD_SECTOR_SIZE)

typedef struct {
    volatile uint8_t *base;
    pthread_mutex_t lock;
    iofunc_attr_t attr;
    iofunc_attr_t fat_attr;
    uint32_t sector_count;
    uint32_t fat_start_sector;
    uint32_t fat_sector_count;
    uint32_t rca;
    int high_capacity;
    int ready;
} bvstk_sd_device_t;

typedef struct {
    io_read_t read;
    struct _xtype_offset offset;
} bvstk_sd_read_offset_t;

typedef struct {
    io_write_t write;
    struct _xtype_offset offset;
} bvstk_sd_write_offset_t;

static bvstk_sd_device_t g_sd;

static volatile uint8_t *sd_ptr(uint32_t offset)
{
    return g_sd.base + offset;
}

static uint8_t sd_read8(uint32_t offset)
{
    uint8_t value = *sd_ptr(offset);

    __sync_synchronize();
    return value;
}

static uint16_t sd_read16(uint32_t offset)
{
    uint16_t value = *(volatile uint16_t *)sd_ptr(offset);

    __sync_synchronize();
    return value;
}

static uint32_t sd_read32(uint32_t offset)
{
    uint32_t value = *(volatile uint32_t *)sd_ptr(offset);

    __sync_synchronize();
    return value;
}

static uint32_t sd_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static void sd_write8(uint32_t offset, uint8_t value)
{
    *sd_ptr(offset) = value;
    __sync_synchronize();
}

static void sd_write16(uint32_t offset, uint16_t value)
{
    *(volatile uint16_t *)sd_ptr(offset) = value;
    __sync_synchronize();
}

static void sd_write32(uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)sd_ptr(offset) = value;
    __sync_synchronize();
}

static uint64_t sd_now_us(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0U;
    }
    return (uint64_t)now.tv_sec * UINT64_C(1000000) +
           (uint64_t)now.tv_nsec / UINT64_C(1000);
}

static int sd_wait8_clear(uint32_t offset, uint8_t mask, uint32_t timeout_us)
{
    uint64_t deadline = sd_now_us() + timeout_us;

    for (;;) {
        if ((sd_read8(offset) & mask) == 0U) {
            return 0;
        }
        if (sd_now_us() >= deadline) {
            errno = ETIMEDOUT;
            return -1;
        }
        usleep(10U);
    }
}

static int sd_wait16_mask(uint32_t offset, uint16_t mask, uint16_t value,
                          uint32_t timeout_us, uint16_t *last)
{
    uint64_t deadline = sd_now_us() + timeout_us;

    for (;;) {
        uint16_t current = sd_read16(offset);

        if (last != NULL) {
            *last = current;
        }
        if ((current & mask) == value) {
            return 0;
        }
        if (sd_now_us() >= deadline) {
            errno = ETIMEDOUT;
            return -1;
        }
        usleep(10U);
    }
}

static int sd_wait_present_clear(uint32_t mask, uint32_t timeout_us)
{
    uint64_t deadline = sd_now_us() + timeout_us;

    for (;;) {
        if ((sd_read32(SD_PRESENT_STATE) & mask) == 0U) {
            return 0;
        }
        if (sd_now_us() >= deadline) {
            errno = ETIMEDOUT;
            return -1;
        }
        usleep(10U);
    }
}

static void sd_log_host_state(const char *phase)
{
    fprintf(stdout,
            "bvstk-sd-raw: %s present=0x%08x power=0x%02x clock=0x%04x"
            " reset=0x%02x status=0x%04x error=0x%04x\n",
            phase,
            sd_read32(SD_PRESENT_STATE),
            sd_read8(SD_POWER_CONTROL),
            sd_read16(SD_CLOCK_CONTROL),
            sd_read8(SD_SOFTWARE_RESET),
            sd_read16(SD_NORMAL_STATUS),
            sd_read16(SD_ERROR_STATUS));
}

static void sd_clear_status(void)
{
    sd_write16(SD_NORMAL_STATUS, SD_STATUS_ALL);
    sd_write16(SD_ERROR_STATUS, SD_ERROR_ALL);
}

static int sd_reset(uint8_t reset)
{
    sd_write8(SD_SOFTWARE_RESET, reset);
    return sd_wait8_clear(SD_SOFTWARE_RESET, reset, SD_TIMEOUT_US);
}

static int sd_set_clock(uint32_t frequency_hz)
{
    uint16_t divider;
    uint16_t clock_reg;

    sd_write16(SD_CLOCK_CONTROL, 0U);
    if (frequency_hz == 0U) {
        return 0;
    }

    /* Zynq-7000 exposes the SDHCI v2 divider.  Select the first power of two
     * which does not exceed the requested card clock. */
    divider = 1U;
    while (divider < 256U &&
           SD_INPUT_CLOCK_HZ / divider > frequency_hz) {
        divider = (uint16_t)(divider << 1U);
    }
    clock_reg = (uint16_t)((divider / 2U) << 8U);
    clock_reg |= SD_CLOCK_INTERNAL_ENABLE;
    sd_write16(SD_CLOCK_CONTROL, clock_reg);
    if (sd_wait16_mask(SD_CLOCK_CONTROL,
                       SD_CLOCK_INTERNAL_STABLE,
                       SD_CLOCK_INTERNAL_STABLE,
                       SD_TIMEOUT_US,
                       NULL) != 0) {
        return -1;
    }
    sd_write16(SD_CLOCK_CONTROL, (uint16_t)(clock_reg | SD_CLOCK_SD_ENABLE));
    return 0;
}

static uint16_t sd_command_flags(uint8_t command, int data_present)
{
    uint16_t flags;

    switch (command) {
    case SD_CMD0:
        flags = SD_CMD_RESPONSE_NONE;
        break;
    case SD_CMD2:
    case SD_CMD9:
        flags = SD_CMD_RESPONSE_136;
        break;
    case SD_CMD3:
        flags = SD_CMD_RESPONSE_48_BUSY | SD_CMD_CRC_CHECK |
                SD_CMD_INDEX_CHECK;
        break;
    case SD_CMD8:
    case SD_CMD7:
    case SD_CMD16:
    case SD_CMD17:
    case SD_CMD24:
    case SD_CMD55:
        flags = SD_CMD_RESPONSE_48 | SD_CMD_CRC_CHECK |
                SD_CMD_INDEX_CHECK;
        break;
    case SD_CMD41:
        flags = SD_CMD_RESPONSE_48;
        break;
    default:
        flags = SD_CMD_RESPONSE_48 | SD_CMD_CRC_CHECK |
                SD_CMD_INDEX_CHECK;
        break;
    }
    if (data_present) {
        flags |= SD_CMD_DATA_PRESENT;
    }
    return flags;
}

static int sd_issue_command(uint8_t command, uint32_t argument,
                            uint16_t block_count, int data_present,
                            int read_transfer, uint32_t response[4])
{
    uint16_t transfer_mode = 0U;
    uint16_t command_reg;
    uint16_t status;

    if (sd_wait_present_clear(SD_PRESENT_CMD_INHIBIT |
                              (data_present ? SD_PRESENT_DATA_INHIBIT : 0U),
                              SD_TIMEOUT_US) != 0) {
        fprintf(stderr,
                "bvstk-sd-raw: CMD%u bus busy present=0x%08x error=0x%04x\n",
                command, sd_read32(SD_PRESENT_STATE),
                sd_read16(SD_ERROR_STATUS));
        return -1;
    }

    sd_write16(SD_BLK_SIZE, SD_SECTOR_SIZE);
    sd_write16(SD_BLK_COUNT, block_count);
    sd_write8(SD_TIMEOUT_CONTROL, 0x0EU);
    sd_write32(SD_ARGUMENT, argument);
    sd_clear_status();

    if (data_present) {
        transfer_mode = SD_XFER_BLOCK_COUNT_ENABLE;
        if (read_transfer) {
            transfer_mode |= SD_XFER_READ;
        }
        if (block_count > 1U) {
            transfer_mode |= SD_XFER_MULTI;
        }
    }
    command_reg = (uint16_t)(((uint16_t)command << 8U) |
                             sd_command_flags(command, data_present));
    sd_write32(SD_XFER_MODE,
               ((uint32_t)command_reg << 16U) | transfer_mode);

    {
        uint64_t deadline = sd_now_us() + SD_TIMEOUT_US;

        for (;;) {
            status = sd_read16(SD_NORMAL_STATUS);
            if ((status & SD_STATUS_ERROR) != 0U) {
                uint16_t error = sd_read16(SD_ERROR_STATUS);

                fprintf(stderr,
                        "bvstk-sd-raw: CMD%u error normal=0x%04x error=0x%04x\n",
                        command, status, error);
                sd_clear_status();
                errno = EIO;
                return -1;
            }
            if ((status & SD_STATUS_COMMAND_COMPLETE) != 0U) {
                break;
            }
            if (sd_now_us() >= deadline) {
                fprintf(stderr,
                        "bvstk-sd-raw: CMD%u timeout normal=0x%04x state=0x%08x\n",
                        command, status, sd_read32(SD_PRESENT_STATE));
                errno = ETIMEDOUT;
                return -1;
            }
            usleep(10U);
        }
    }

    if (response != NULL) {
        response[0] = sd_read32(SD_RESPONSE0);
        response[1] = sd_read32(SD_RESPONSE1);
        response[2] = sd_read32(SD_RESPONSE2);
        response[3] = sd_read32(SD_RESPONSE3);
    }
    sd_write16(SD_NORMAL_STATUS, SD_STATUS_COMMAND_COMPLETE);
    return 0;
}

static int sd_wait_data_status(uint16_t wanted, uint16_t *status_out)
{
    uint64_t deadline = sd_now_us() + SD_TIMEOUT_US;

    for (;;) {
        uint16_t status = sd_read16(SD_NORMAL_STATUS);

        if ((status & SD_STATUS_ERROR) != 0U) {
            uint16_t error = sd_read16(SD_ERROR_STATUS);

            fprintf(stderr,
                    "bvstk-sd-raw: data error normal=0x%04x error=0x%04x\n",
                    status, error);
            sd_clear_status();
            errno = EIO;
            return -1;
        }
        if ((status & wanted) != 0U) {
            if (status_out != NULL) {
                *status_out = status;
            }
            return 0;
        }
        if (sd_now_us() >= deadline) {
            fprintf(stderr,
                    "bvstk-sd-raw: data timeout normal=0x%04x state=0x%08x\n",
                    status, sd_read32(SD_PRESENT_STATE));
            errno = ETIMEDOUT;
            return -1;
        }
        usleep(10U);
    }
}

static int sd_transfer_one(uint32_t sector, uint8_t *buffer, int write)
{
    uint32_t argument = g_sd.high_capacity
                        ? sector
                        : sector * SD_SECTOR_SIZE;
    uint16_t status;
    uint32_t word;
    uint32_t index;
    int result;

    if (write) {
        result = sd_issue_command(SD_CMD24, argument, 1U, 1, 0, NULL);
    } else {
        result = sd_issue_command(SD_CMD17, argument, 1U, 1, 1, NULL);
    }
    if (result != 0) {
        return -1;
    }

    if (sd_wait_data_status(write ? SD_STATUS_BUFFER_WRITE_READY
                                  : SD_STATUS_BUFFER_READ_READY,
                            &status) != 0) {
        return -1;
    }
    (void)status;
    for (index = 0U; index < SD_SECTOR_SIZE / sizeof(uint32_t); ++index) {
        if (write) {
            memcpy(&word, &buffer[index * sizeof(uint32_t)], sizeof(word));
            sd_write32(SD_BUFFER, word);
        } else {
            word = sd_read32(SD_BUFFER);
            memcpy(&buffer[index * sizeof(uint32_t)], &word, sizeof(word));
        }
    }
    sd_write16(SD_NORMAL_STATUS,
               (uint16_t)(write ? SD_STATUS_BUFFER_WRITE_READY
                                : SD_STATUS_BUFFER_READ_READY));
    if (sd_wait16_mask(SD_NORMAL_STATUS,
                       (uint16_t)(SD_STATUS_TRANSFER_COMPLETE |
                                  SD_STATUS_ERROR),
                       SD_STATUS_TRANSFER_COMPLETE,
                       SD_TIMEOUT_US,
                       &status) != 0) {
        uint16_t error = sd_read16(SD_ERROR_STATUS);

        fprintf(stderr,
                "bvstk-sd-raw: sector %" PRIu32 " completion normal=0x%04x error=0x%04x\n",
                sector, status, error);
        sd_clear_status();
        return -1;
    }
    sd_write16(SD_NORMAL_STATUS, SD_STATUS_TRANSFER_COMPLETE);
    return 0;
}

static int sd_read_blocks(uint32_t sector, uint8_t *buffer, uint32_t count)
{
    uint32_t index;

    if (buffer == NULL || count == 0U ||
        sector >= g_sd.sector_count || count > g_sd.sector_count - sector) {
        errno = EINVAL;
        return -1;
    }
    for (index = 0U; index < count; ++index) {
        if (sd_transfer_one(sector + index,
                            buffer + (size_t)index * SD_SECTOR_SIZE,
                            0) != 0) {
            return -1;
        }
    }
    return 0;
}

static int sd_write_blocks(uint32_t sector, const uint8_t *buffer,
                           uint32_t count)
{
    uint32_t index;

    if (buffer == NULL || count == 0U ||
        sector >= g_sd.sector_count || count > g_sd.sector_count - sector) {
        errno = EINVAL;
        return -1;
    }
    for (index = 0U; index < count; ++index) {
        if (sd_transfer_one(sector + index,
                            (uint8_t *)(buffer + (size_t)index * SD_SECTOR_SIZE),
                            1) != 0) {
            return -1;
        }
    }
    return 0;
}

static int sd_init_card(void)
{
    uint32_t response[4] = { 0U, 0U, 0U, 0U };
    uint32_t ocr = 0U;
    uint32_t csd[4] = { 0U, 0U, 0U, 0U };
    uint32_t count;
    uint64_t deadline;
    uint16_t version;

    version = sd_read16(SD_HOST_CONTROL_VERSION);
    fprintf(stdout,
            "bvstk-sd-raw: host version=0x%04x caps=0x%08x present=0x%08x\n",
            version,
            sd_read32(SD_CAPABILITIES),
            sd_read32(SD_PRESENT_STATE));
    if ((sd_read32(SD_PRESENT_STATE) & SD_PRESENT_CARD_INSERTED) == 0U) {
        errno = ENODEV;
        return -1;
    }

    sd_write8(SD_POWER_CONTROL, 0U);
    usleep(1000U);
    if (sd_reset(SD_RESET_ALL) != 0) {
        fprintf(stderr, "bvstk-sd-raw: host reset failed: %s\n",
                strerror(errno));
        return -1;
    }
    sd_log_host_state("after reset");
    sd_write8(SD_POWER_CONTROL, SD_POWER_3V3);
    usleep(200U);
    sd_log_host_state("after power");
    sd_write8(SD_HOST_CONTROL1, 0U);
    sd_write8(SD_TIMEOUT_CONTROL, 0x0EU);
    sd_write16(SD_NORMAL_STATUS_ENABLE,
               (uint16_t)(SD_STATUS_ALL & ~UINT16_C(0x0100)));
    sd_write16(SD_ERROR_STATUS_ENABLE, SD_ERROR_ALL);
    sd_write16(SD_NORMAL_SIGNAL_ENABLE, 0U);
    sd_write16(SD_ERROR_SIGNAL_ENABLE, 0U);
    sd_clear_status();
    if (sd_set_clock(400000U) != 0) {
        fprintf(stderr, "bvstk-sd-raw: initial clock failed: %s\n",
                strerror(errno));
        return -1;
    }
    sd_log_host_state("after 400kHz clock");
    usleep(10000U);

    if (sd_issue_command(SD_CMD0, 0U, 0U, 0, 0, NULL) != 0 ||
        sd_issue_command(SD_CMD8, SD_CMD8_ARGUMENT, 0U, 0, 0, response) != 0) {
        fprintf(stderr, "bvstk-sd-raw: card reset/interface condition failed\n");
        return -1;
    }
    fprintf(stdout, "bvstk-sd-raw: CMD8 response=0x%08x\n", response[0]);
    if (response[0] != SD_CMD8_ARGUMENT) {
        errno = ENOTSUP;
        return -1;
    }

    deadline = sd_now_us() + SD_INIT_TIMEOUT_US;
    do {
        if (sd_issue_command(SD_CMD55, 0U, 0U, 0, 0, NULL) != 0 ||
            sd_issue_command(SD_CMD41,
                             SD_OCR_POWER_UP | SD_OCR_HIGH_CAPACITY | SD_OCR_3V3,
                             0U, 0, 0, response) != 0) {
            return -1;
        }
        ocr = response[0];
        if ((ocr & SD_OCR_POWER_UP) != 0U) {
            break;
        }
        usleep(1000U);
    } while (sd_now_us() < deadline);
    if ((ocr & SD_OCR_POWER_UP) == 0U) {
        errno = ETIMEDOUT;
        return -1;
    }
    g_sd.high_capacity = (ocr & SD_OCR_HIGH_CAPACITY) != 0U;
    fprintf(stdout, "bvstk-sd-raw: ACMD41 response=0x%08x high_capacity=%d\n",
            ocr, g_sd.high_capacity);

    if (sd_issue_command(SD_CMD2, 0U, 0U, 0, 0, response) != 0 ||
        sd_issue_command(SD_CMD3, 0U, 0U, 0, 0, response) != 0) {
        return -1;
    }
    g_sd.rca = response[0] & UINT32_C(0xFFFF0000);
    if (g_sd.rca == 0U) {
        errno = EIO;
        return -1;
    }
    if (sd_issue_command(SD_CMD9, g_sd.rca, 0U, 0, 0, csd) != 0 ||
        sd_issue_command(SD_CMD7, g_sd.rca, 0U, 0, 0, NULL) != 0) {
        return -1;
    }

    if (g_sd.high_capacity) {
        count = (((csd[1] & UINT32_C(0x3FFFFF00)) >> 8U) + 1U) * 1024U;
    } else {
        uint32_t block_length = 1U << ((csd[2] & UINT32_C(0x00000F00)) >> 8U);
        uint32_t multiplier = 1U << (((csd[1] & UINT32_C(0x0000007F)) >> 7U) + 2U);
        uint32_t device_size = ((csd[1] & UINT32_C(0x03C00000)) >> 22U) |
                               ((csd[2] & UINT32_C(0x0000003F)) << 10U);

        count = ((device_size + 1U) * multiplier * block_length) /
                SD_SECTOR_SIZE;
        if (sd_issue_command(SD_CMD16, SD_SECTOR_SIZE, 0U, 0, 0, NULL) != 0) {
            return -1;
        }
    }
    if (count == 0U) {
        errno = EIO;
        return -1;
    }
    g_sd.sector_count = count;
    sd_write16(SD_BLK_SIZE, SD_SECTOR_SIZE);

    /* Keep the card in its conservative 1-bit mode while proving the
     * physical path.  The 25 MHz default-speed clock is within the AX7020
     * level-shifter limits and is fast enough for the filesystem backend. */
    if (sd_set_clock(25000000U) != 0) {
        return -1;
    }

    {
        uint8_t mbr[SD_SECTOR_SIZE];
        uint32_t partition;

        if (sd_read_blocks(0U, mbr, 1U) != 0 ||
            mbr[510] != 0x55U || mbr[511] != 0xAAU) {
            errno = ENOTSUP;
            return -1;
        }
        for (partition = 0U; partition < 4U; ++partition) {
            const uint8_t *entry = &mbr[446U + partition * 16U];
            uint32_t start = sd_le32(&entry[8]);
            uint32_t sectors = sd_le32(&entry[12]);
            uint8_t type = entry[4];

            if ((type == 0x01U || type == 0x04U || type == 0x06U ||
                 type == 0x0BU || type == 0x0CU || type == 0x0EU ||
                 type == 0x1BU || type == 0x1EU) &&
                sectors != 0U && start < g_sd.sector_count &&
                sectors <= g_sd.sector_count - start) {
                g_sd.fat_start_sector = start;
                g_sd.fat_sector_count = sectors;
                break;
            }
        }
    }
    if (g_sd.fat_sector_count == 0U) {
        errno = ENODEV;
        return -1;
    }
    g_sd.ready = 1;
    fprintf(stdout,
            "bvstk-sd-raw: ready rca=0x%08x sectors=%" PRIu32
            " fat_start=%" PRIu32 " fat_sectors=%" PRIu32
            " clock=25MHz width=1\n",
            g_sd.rca, g_sd.sector_count, g_sd.fat_start_sector,
            g_sd.fat_sector_count);
    return 0;
}

static void sd_view_for_ocb(const iofunc_ocb_t *ocb, uint32_t *start_sector,
                            uint32_t *sector_count)
{
    *start_sector = 0U;
    *sector_count = g_sd.sector_count;
    if (ocb->attr == &g_sd.fat_attr) {
        *start_sector = g_sd.fat_start_sector;
        *sector_count = g_sd.fat_sector_count;
    }
}

static int sd_io_read(resmgr_context_t *ctp, io_read_t *msg,
                      iofunc_ocb_t *ocb)
{
    uint32_t view_start_sector;
    uint32_t view_sector_count;
    uint64_t offset;
    uint64_t view_bytes;
    size_t requested;
    size_t available;
    size_t count;
    int update_ocb;
    size_t processed = 0U;
    uint8_t buffer[SD_IO_CHUNK];
    int verify;

    verify = iofunc_read_verify(ctp, msg, ocb, NULL);
    if (verify != EOK) {
        return verify;
    }
    sd_view_for_ocb(ocb, &view_start_sector, &view_sector_count);
    view_bytes = (uint64_t)view_sector_count * SD_SECTOR_SIZE;
    switch (msg->i.xtype & _IO_XTYPE_MASK) {
    case _IO_XTYPE_NONE:
        if (ocb->offset < 0) {
            return _RESMGR_ERRNO(ENOSPC);
        }
        offset = (uint64_t)ocb->offset;
        update_ocb = 1;
        break;
    case _IO_XTYPE_OFFSET:
        if (((bvstk_sd_read_offset_t *)msg)->offset.offset < 0) {
            return _RESMGR_ERRNO(ENOSPC);
        }
        offset = (uint64_t)((bvstk_sd_read_offset_t *)msg)->offset.offset;
        update_ocb = 0;
        break;
    default:
        return _RESMGR_ERRNO(ENOSYS);
    }
    if (offset > view_bytes) {
        return _RESMGR_ERRNO(ENOSPC);
    }
    if (offset == view_bytes) {
        return _IO_SET_READ_NBYTES(ctp, 0);
    }
    requested = (size_t)msg->i.nbytes;
    available = (size_t)(view_bytes - offset);
    count = requested < available ? requested : available;
    if (count == 0U || (offset % SD_SECTOR_SIZE) != 0U ||
        (count % SD_SECTOR_SIZE) != 0U) {
        return _RESMGR_ERRNO(EINVAL);
    }

    if (pthread_mutex_lock(&g_sd.lock) != 0) {
        return _RESMGR_ERRNO(EBUSY);
    }
    while (processed < count) {
        size_t chunk = count - processed;

        if (chunk > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }
        if (sd_read_blocks(view_start_sector +
                           (uint32_t)((offset + processed) / SD_SECTOR_SIZE),
                           buffer, (uint32_t)(chunk / SD_SECTOR_SIZE)) != 0 ||
            resmgr_msgwrite(ctp, buffer, (int)chunk, (int)processed) !=
            (int)chunk) {
            int saved_errno = errno == 0 ? EIO : errno;

            (void)pthread_mutex_unlock(&g_sd.lock);
            return _RESMGR_ERRNO(saved_errno);
        }
        processed += chunk;
    }
    (void)pthread_mutex_unlock(&g_sd.lock);
    if (update_ocb) {
        ocb->offset += (off_t)processed;
    }
    _IO_SET_READ_NBYTES(ctp, processed);
    return EOK;
}

static int sd_io_write(resmgr_context_t *ctp, io_write_t *msg,
                       iofunc_ocb_t *ocb)
{
    uint32_t view_start_sector;
    uint32_t view_sector_count;
    uint64_t offset;
    uint64_t view_bytes;
    size_t requested;
    size_t available;
    size_t count;
    size_t processed = 0U;
    size_t message_offset;
    int update_ocb;
    uint8_t buffer[SD_IO_CHUNK];
    int verify;

    verify = iofunc_write_verify(ctp, msg, ocb, NULL);
    if (verify != EOK) {
        return verify;
    }
    if ((ocb->ioflag & O_ACCMODE) == O_RDONLY) {
        return _RESMGR_ERRNO(EBADF);
    }
    sd_view_for_ocb(ocb, &view_start_sector, &view_sector_count);
    view_bytes = (uint64_t)view_sector_count * SD_SECTOR_SIZE;
    switch (msg->i.xtype & _IO_XTYPE_MASK) {
    case _IO_XTYPE_NONE:
        if (ocb->offset < 0) {
            return _RESMGR_ERRNO(ENOSPC);
        }
        offset = (uint64_t)ocb->offset;
        message_offset = sizeof(msg->i);
        update_ocb = 1;
        break;
    case _IO_XTYPE_OFFSET:
        if (((bvstk_sd_write_offset_t *)msg)->offset.offset < 0) {
            return _RESMGR_ERRNO(ENOSPC);
        }
        offset = (uint64_t)((bvstk_sd_write_offset_t *)msg)->offset.offset;
        message_offset = sizeof(msg->i) + sizeof(struct _xtype_offset);
        update_ocb = 0;
        break;
    default:
        return _RESMGR_ERRNO(ENOSYS);
    }
    if (offset >= view_bytes) {
        return _RESMGR_ERRNO(ENOSPC);
    }
    requested = (size_t)msg->i.nbytes;
    available = (size_t)(view_bytes - offset);
    count = requested < available ? requested : available;
    if (count == 0U || (offset % SD_SECTOR_SIZE) != 0U ||
        (count % SD_SECTOR_SIZE) != 0U) {
        return _RESMGR_ERRNO(EINVAL);
    }

    if (pthread_mutex_lock(&g_sd.lock) != 0) {
        return _RESMGR_ERRNO(EBUSY);
    }
    while (processed < count) {
        size_t chunk = count - processed;

        if (chunk > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }
        if (resmgr_msgread(ctp, buffer, (int)chunk,
                           (int)message_offset + (int)processed) !=
                (int)chunk ||
            sd_write_blocks(view_start_sector +
                            (uint32_t)((offset + processed) / SD_SECTOR_SIZE),
                            buffer, (uint32_t)(chunk / SD_SECTOR_SIZE)) != 0) {
            int saved_errno = errno == 0 ? EIO : errno;

            (void)pthread_mutex_unlock(&g_sd.lock);
            return _RESMGR_ERRNO(saved_errno);
        }
        processed += chunk;
    }
    (void)pthread_mutex_unlock(&g_sd.lock);
    if (update_ocb) {
        ocb->offset += (off_t)processed;
    }
    _IO_SET_WRITE_NBYTES(ctp, processed);
    return EOK;
}

static int sd_backend_init(void)
{
    void *mapped;

    if (ThreadCtl(_NTO_TCTL_IO, NULL) == -1) {
        return -1;
    }
    mapped = mmap_device_memory(NULL,
                                SD_MAP_SIZE,
                                PROT_READ | PROT_WRITE | PROT_NOCACHE,
                                0,
                                SD_BASE);
    if (mapped == MAP_FAILED) {
        return -1;
    }
    memset(&g_sd, 0, sizeof(g_sd));
    g_sd.base = (volatile uint8_t *)mapped;
    if (pthread_mutex_init(&g_sd.lock, NULL) != 0) {
        (void)munmap(mapped, SD_MAP_SIZE);
        errno = EBUSY;
        return -1;
    }
    if (sd_init_card() != 0) {
        fprintf(stderr, "bvstk-sd-raw: card initialization failed: %s\n",
                strerror(errno));
        return -1;
    }
    return 0;
}

static int sd_probe(void)
{
    uint8_t sector[SD_SECTOR_SIZE];
    uint32_t index;

    if (sd_read_blocks(0U, sector, 1U) != 0) {
        return -1;
    }
    fprintf(stdout, "bvstk-sd-raw: sector0:");
    for (index = 0U; index < 64U; ++index) {
        if ((index % 16U) == 0U) {
            fputc(index == 0U ? ' ' : '\n', stdout);
        }
        fprintf(stdout, "%02x ", sector[index]);
    }
    fprintf(stdout, "\n");
    fprintf(stdout, "bvstk-sd-raw: signature=0x%02x%02x fat_hint=%s\n",
            sector[511], sector[510],
            (sector[510] == 0x55U && sector[511] == 0xAAU) ? "yes" : "no");
    for (index = 0U; index < 4U; ++index) {
        const uint8_t *entry = &sector[446U + index * 16U];

        fprintf(stdout,
                "bvstk-sd-raw: partition%u type=0x%02x start=%" PRIu32
                " sectors=%" PRIu32 "\n",
                index, entry[4], sd_le32(&entry[8]), sd_le32(&entry[12]));
    }
    return 0;
}

int main(int argc, char **argv)
{
    dispatch_t *dispatch;
    dispatch_context_t *context;
    resmgr_attr_t resmgr_attr;
    resmgr_connect_funcs_t connect_funcs;
    resmgr_io_funcs_t io_funcs;
    int attach_id;
    int fat_attach_id;

    if (sd_backend_init() != 0) {
        return EXIT_FAILURE;
    }
    if (argc > 1 && strcmp(argv[1], "--probe") == 0) {
        return sd_probe() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    memset(&g_sd.attr, 0, sizeof(g_sd.attr));
    iofunc_attr_init(&g_sd.attr, S_IFREG | 0660, NULL, NULL);
    g_sd.attr.nbytes = (off_t)((uint64_t)g_sd.sector_count * SD_SECTOR_SIZE);
    memset(&g_sd.fat_attr, 0, sizeof(g_sd.fat_attr));
    iofunc_attr_init(&g_sd.fat_attr, S_IFREG | 0660, NULL, NULL);
    g_sd.fat_attr.nbytes =
        (off_t)((uint64_t)g_sd.fat_sector_count * SD_SECTOR_SIZE);
    dispatch = dispatch_create();
    if (dispatch == NULL) {
        fprintf(stderr, "bvstk-sd-raw: dispatch_create failed: %s\n",
                strerror(errno));
        return EXIT_FAILURE;
    }
    memset(&resmgr_attr, 0, sizeof(resmgr_attr));
    resmgr_attr.nparts_max = 2U;
    resmgr_attr.msg_max_size = SD_MAX_TRANSFER_BYTES;
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs,
                     _RESMGR_IO_NFUNCS, &io_funcs);
    io_funcs.read = sd_io_read;
    io_funcs.write = sd_io_write;
    attach_id = resmgr_attach(dispatch, &resmgr_attr,
                              "/dev/bvstk-sd-raw", _FTYPE_ANY, 0U,
                              &connect_funcs, &io_funcs, &g_sd.attr);
    if (attach_id == -1) {
        fprintf(stderr, "bvstk-sd-raw: resmgr_attach failed: %s\n",
                strerror(errno));
        return EXIT_FAILURE;
    }
    fat_attach_id = resmgr_attach(dispatch, &resmgr_attr,
                                   "/dev/bvstk-sd-fat", _FTYPE_ANY, 0U,
                                   &connect_funcs, &io_funcs,
                                   &g_sd.fat_attr);
    if (fat_attach_id == -1) {
        fprintf(stderr, "bvstk-sd-raw: FAT attach failed: %s\n",
                strerror(errno));
        return EXIT_FAILURE;
    }
    fprintf(stdout,
            "bvstk-sd-raw: raw=/dev/bvstk-sd-raw fat=/dev/bvstk-sd-fat"
            " sectors=%" PRIu32 "\n",
            g_sd.sector_count);
    context = dispatch_context_alloc(dispatch);
    if (context == NULL) {
        fprintf(stderr, "bvstk-sd-raw: dispatch_context_alloc failed: %s\n",
                strerror(errno));
        return EXIT_FAILURE;
    }
    for (;;) {
        context = dispatch_block(context);
        if (context == NULL) {
            context = dispatch_context_alloc(dispatch);
            if (context == NULL) {
                return EXIT_FAILURE;
            }
            continue;
        }
        (void)dispatch_handler(context);
    }
}
