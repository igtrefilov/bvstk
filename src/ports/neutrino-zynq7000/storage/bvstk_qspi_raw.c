/*
 * Neutrino raw backend for the FatFs-compatible QSPI window.
 *
 * The FreeRTOS port exposes the QSPI flash region starting at
 * QSPI_FS_BASE_BYTES as a FAT volume.  This resource manager exposes the
 * same region as a seekable block-like file.  devb-loopback then supplies
 * the standard Neutrino block layer and fs-dos mounts the existing FAT
 * volume without changing its on-flash layout.
 */

#include "hardware/boards/ax7020/bvstk_qspi_layout.h"

#include <errno.h>
#include <fcntl.h>
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
#include <unistd.h>

#define BVSTK_QSPI_BASE                 ((uintptr_t)0xE000D000U)
#define BVSTK_QSPI_MAP_SIZE             ((size_t)0x1000U)
#define BVSTK_SLCR_BASE                 ((uintptr_t)0xF8000000U)
#define BVSTK_SLCR_MAP_SIZE             ((size_t)0x1000U)

#define QSPI_CR                         0x00U
#define QSPI_SR                         0x04U
#define QSPI_ER                         0x14U
#define QSPI_TXD_00                     0x1CU
#define QSPI_RXD                        0x20U
#define QSPI_TXWR                       0x28U
#define QSPI_RXWR                       0x2CU
#define QSPI_TXD_01                     0x80U
#define QSPI_TXD_10                     0x84U
#define QSPI_TXD_11                     0x88U
#define QSPI_LQSPI_CR                   0xA0U

#define QSPI_CR_MANSTRT                 UINT32_C(0x00010000)
#define QSPI_CR_MANSTRTEN               UINT32_C(0x00008000)
#define QSPI_CR_SSFORCE                 UINT32_C(0x00004000)
#define QSPI_CR_SSCTRL                  UINT32_C(0x00000400)
#define QSPI_CR_DATA_SZ                 UINT32_C(0x000000C0)
#define QSPI_CR_IFMODE                  UINT32_C(0x80000000)
#define QSPI_CR_PRESC_SHIFT             3U
#define QSPI_CR_MSTREN                  UINT32_C(0x00000001)
#define QSPI_CR_HOLD_B                  UINT32_C(0x00080000)

#define QSPI_SR_RXNEMPTY                UINT32_C(0x00000010)
#define QSPI_SR_TXOW                    UINT32_C(0x00000004)
#define QSPI_SR_TXUF                    UINT32_C(0x00000040)
#define QSPI_SR_RXOVR                   UINT32_C(0x00000001)
#define QSPI_SR_WRITE_TO_CLEAR          UINT32_C(0x00000041)
#define QSPI_IDR                         0x0CU
#define QSPI_IXR_DISABLE_ALL             UINT32_C(0x0000007D)
#define QSPI_CR_RESET_SET                (QSPI_CR_IFMODE | QSPI_CR_SSCTRL | \
                                          QSPI_CR_DATA_SZ | QSPI_CR_MSTREN | \
                                          QSPI_CR_SSFORCE | QSPI_CR_HOLD_B)
#define QSPI_CR_RESET_CLEAR              UINT32_C(0x0401803E)
#define SLCR_LOCK                        0x004U
#define SLCR_UNLOCK                      0x008U
#define SLCR_LOCKSTA                     0x00CU
#define SLCR_LQSPI_RST_CTRL              0x230U
#define SLCR_LQSPI_RST_MASK              UINT32_C(0x00000003)
#define SLCR_LOCK_MASK                   UINT32_C(0x0000767B)
#define SLCR_UNLOCK_MASK                 UINT32_C(0x0000DF0D)

#define QSPI_ER_ENABLE                  UINT32_C(0x00000001)
#define QSPI_FIFO_DEPTH                 63U

#define QSPI_CMD_WRITE_ENABLE           UINT8_C(0x06)
#define QSPI_CMD_READ_STATUS            UINT8_C(0x05)
#define QSPI_CMD_PAGE_PROGRAM           UINT8_C(0x02)
#define QSPI_CMD_READ_DATA              UINT8_C(0x03)
#define QSPI_CMD_SECTOR_ERASE_4K        UINT8_C(0x20)
#define QSPI_CMD_READ_ID                UINT8_C(0x9F)
#define QSPI_STATUS_WIP                 UINT8_C(0x01)
#define QSPI_STATUS_WEL                 UINT8_C(0x02)

#define QSPI_TIMEOUT_POLLS              2000000U
#define QSPI_READY_TIMEOUT_MS           5000U
#define QSPI_IO_CHUNK                   QSPI_FLASH_SECTOR_SIZE
#define QSPI_IO_BLOCK_SIZE              512U

/*
 * Keep one manual transaction below the FIFO refill boundary.  The Zynq
 * QSPI controller can hold 63 words, but a transaction which fills the FIFO
 * completely may underflow while the CPU is draining RXD and preparing the
 * next batch.  Splitting normal reads/page programs into 128-byte pieces
 * keeps the transaction bounded and leaves the controller in a deterministic
 * state on every chip-select assertion.
 */
#define QSPI_TRANSFER_DATA_MAX          128U
#define QSPI_TRANSFER_HEADER_SIZE       4U
#define QSPI_TRANSFER_MAX              (QSPI_TRANSFER_HEADER_SIZE + \
                                         QSPI_TRANSFER_DATA_MAX)

typedef struct {
    volatile uint8_t *base;
    pthread_mutex_t lock;
    iofunc_attr_t attr;
} bvstk_qspi_device_t;

typedef struct {
    io_read_t read;
    struct _xtype_offset offset;
} bvstk_qspi_read_offset_t;

typedef struct {
    io_write_t write;
    struct _xtype_offset offset;
} bvstk_qspi_write_offset_t;

static bvstk_qspi_device_t g_qspi;
static uint32_t g_qspi_config;

static volatile uint32_t *qspi_reg(uint32_t offset)
{
    return (volatile uint32_t *)(g_qspi.base + offset);
}

static uint32_t qspi_read_reg(uint32_t offset)
{
    uint32_t value = *qspi_reg(offset);
    __sync_synchronize();
    return value;
}

static void qspi_write_reg(uint32_t offset, uint32_t value)
{
    *qspi_reg(offset) = value;
    __sync_synchronize();
}

static uint32_t slcr_read_reg(volatile uint8_t *slcr, uint32_t offset)
{
    uint32_t value = *(volatile uint32_t *)(slcr + offset);
    __sync_synchronize();
    return value;
}

static void slcr_write_reg(volatile uint8_t *slcr, uint32_t offset,
                           uint32_t value)
{
    *(volatile uint32_t *)(slcr + offset) = value;
    __sync_synchronize();
}

static int qspi_controller_reset(void)
{
    volatile uint8_t *slcr;
    uint32_t config;
    uint32_t was_locked;

    slcr = (volatile uint8_t *)mmap_device_memory(
        NULL, BVSTK_SLCR_MAP_SIZE, PROT_READ | PROT_WRITE | PROT_NOCACHE,
        0, BVSTK_SLCR_BASE);
    if (slcr == MAP_FAILED) {
        return -1;
    }

    qspi_write_reg(QSPI_IDR, QSPI_IXR_DISABLE_ALL);
    qspi_write_reg(QSPI_ER, 0U);
    config = qspi_read_reg(QSPI_CR);
    config |= QSPI_CR_SSCTRL | QSPI_CR_SSFORCE;
    qspi_write_reg(QSPI_CR, config);
    qspi_write_reg(QSPI_TXWR, 1U);
    qspi_write_reg(QSPI_RXWR, 1U);
    while ((qspi_read_reg(QSPI_SR) & QSPI_SR_RXNEMPTY) != 0U) {
        (void)qspi_read_reg(QSPI_RXD);
    }
    (void)qspi_read_reg(QSPI_SR);
    qspi_write_reg(QSPI_SR, QSPI_SR_WRITE_TO_CLEAR);

    config = qspi_read_reg(QSPI_CR);
    config |= QSPI_CR_RESET_SET;
    config &= ~QSPI_CR_RESET_CLEAR;
    qspi_write_reg(QSPI_CR, config);
    qspi_write_reg(QSPI_LQSPI_CR, 0U);

    /* The LQSPI reset register is write-protected by SLCR.  This is the
     * same unlock/reset/lock sequence used by XQspiPs_Abort(). */
    was_locked = slcr_read_reg(slcr, SLCR_LOCKSTA) != 0U;
    if (was_locked) {
        slcr_write_reg(slcr, SLCR_UNLOCK, SLCR_UNLOCK_MASK);
    }
    slcr_write_reg(slcr, SLCR_LQSPI_RST_CTRL, SLCR_LQSPI_RST_MASK);
    slcr_write_reg(slcr, SLCR_LQSPI_RST_CTRL, 0U);
    if (was_locked) {
        slcr_write_reg(slcr, SLCR_LOCK, SLCR_LOCK_MASK);
    }
    (void)munmap((void *)slcr, BVSTK_SLCR_MAP_SIZE);
    return 0;
}

static int qspi_reconfigure(void)
{
    if (qspi_controller_reset() != 0) {
        return -1;
    }
    qspi_write_reg(QSPI_CR, g_qspi_config);
    qspi_write_reg(QSPI_RXWR, 1U);
    qspi_write_reg(QSPI_TXWR, 1U);
    return 0;
}

static uint32_t pack_word(const uint8_t *data, size_t offset, size_t length)
{
    uint32_t word = 0U;
    size_t index;

    for (index = 0U; index < length && index < 4U; ++index) {
        word |= (uint32_t)data[offset + index] << (index * 8U);
    }
    return word;
}

static void unpack_word(uint8_t *data, size_t offset, size_t length,
                        uint32_t word)
{
    size_t index;

    for (index = 0U; index < length && index < 4U; ++index) {
        data[offset + index] = (uint8_t)(word >> (index * 8U));
    }
}

static int qspi_wait_rx_word(uint32_t *word)
{
    uint32_t poll;

    for (poll = 0U; poll < QSPI_TIMEOUT_POLLS; ++poll) {
        uint32_t status = qspi_read_reg(QSPI_SR);

        if ((status & (QSPI_SR_TXUF | QSPI_SR_RXOVR)) != 0U) {
            errno = EIO;
            return -1;
        }
        if ((status & QSPI_SR_RXNEMPTY) != 0U) {
            *word = qspi_read_reg(QSPI_RXD);
            return 0;
        }
    }

    errno = ETIMEDOUT;
    return -1;
}

static void qspi_set_slave_selected(int selected)
{
    uint32_t config = qspi_read_reg(QSPI_CR);

    if (selected) {
        config &= ~QSPI_CR_SSCTRL;
    } else {
        config |= QSPI_CR_SSCTRL;
    }
    qspi_write_reg(QSPI_CR, config);
}

static int qspi_wait_tx_fifo_ready(void)
{
    uint32_t poll;

    for (poll = 0U; poll < QSPI_TIMEOUT_POLLS; ++poll) {
        uint32_t status = qspi_read_reg(QSPI_SR);

        if ((status & QSPI_SR_TXOW) != 0U) {
            return 0;
        }
    }

    errno = ETIMEDOUT;
    return -1;
}

/*
 * Transfer a single manually-started SPI transaction.
 *
 * The Zynq QSPI FIFO has dedicated registers for the first 1/2/3 bytes of a
 * flash instruction and a four-byte data register for the remainder.  The
 * caller keeps the complete transaction in tx; for a read, bytes after the
 * first four are replaced by 0xFF clock-out bytes.
 */
static int qspi_transfer(const uint8_t *tx, uint8_t *rx, size_t count)
{
    size_t first_length;
    size_t offset;
    size_t total_words;
    size_t next_word;
    size_t received_words;

    if (tx == NULL || count == 0U || count > QSPI_TRANSFER_MAX) {
        errno = EINVAL;
        return -1;
    }

    first_length = count <= 4U ? count : count % 4U;
    if (first_length == 0U) {
        first_length = 4U;
    }
    offset = first_length;
    total_words = 1U + (count - first_length + 3U) / 4U;
    next_word = 0U;
    received_words = 0U;

    while ((qspi_read_reg(QSPI_SR) & QSPI_SR_RXNEMPTY) != 0U) {
        (void)qspi_read_reg(QSPI_RXD);
    }
    (void)qspi_read_reg(QSPI_SR);
    qspi_write_reg(QSPI_SR, QSPI_SR_WRITE_TO_CLEAR);
    qspi_write_reg(QSPI_RXWR, 1U);
    qspi_write_reg(QSPI_TXWR, 1U);
    qspi_write_reg(QSPI_ER, QSPI_ER_ENABLE);
    qspi_set_slave_selected(1);

    /*
     * For a command whose length is not a multiple of four the first
     * instruction bytes go through the dedicated TXD_01/10/11 register.
     * For longer transactions the controller must transmit that word before
     * the following TXD_00 words are put into the FIFO; this is the same
     * TXD1/TXD0 hand-off used by the Xilinx QSPI driver.
     */
    if (first_length != 4U && count > 4U) {
        uint32_t word = pack_word(tx, 0U, first_length);
        uint32_t tx_offset = first_length == 1U ? QSPI_TXD_01 :
                             first_length == 2U ? QSPI_TXD_10 : QSPI_TXD_11;

        qspi_write_reg(tx_offset, word);
        next_word = 1U;
        {
            uint32_t config = qspi_read_reg(QSPI_CR);

            if ((config & QSPI_CR_MANSTRTEN) != 0U) {
                qspi_write_reg(QSPI_CR, config | QSPI_CR_MANSTRT);
            }
        }
        if (qspi_wait_tx_fifo_ready() != 0) {
            qspi_set_slave_selected(0);
            qspi_write_reg(QSPI_ER, 0U);
            return -1;
        }
    }

    while (received_words < total_words) {
        size_t queued_words = next_word - received_words;

        while (next_word < total_words && queued_words < QSPI_FIFO_DEPTH) {
            uint32_t word;
            size_t word_offset;
            size_t word_length;
            uint32_t tx_offset;

            if (next_word == 0U) {
                word_offset = 0U;
                word_length = first_length;
                tx_offset = first_length == 1U ? QSPI_TXD_01 :
                            first_length == 2U ? QSPI_TXD_10 :
                            first_length == 3U ? QSPI_TXD_11 : QSPI_TXD_00;
            } else {
                word_offset = offset;
                word_length = count - offset;
                if (word_length > 4U) {
                    word_length = 4U;
                }
                tx_offset = QSPI_TXD_00;
            }

            if (rx != NULL && next_word != 0U && word_offset >= 4U) {
                word = UINT32_C(0xFFFFFFFF);
            } else {
                word = pack_word(tx, word_offset, word_length);
            }
            qspi_write_reg(tx_offset, word);

            if (next_word != 0U) {
                offset += word_length;
            }
            ++next_word;
            ++queued_words;
        }

        {
            uint32_t config = qspi_read_reg(QSPI_CR);

            if ((config & QSPI_CR_MANSTRTEN) != 0U) {
                qspi_write_reg(QSPI_CR, config | QSPI_CR_MANSTRT);
            }
        }

        while (received_words < next_word) {
            uint32_t word;
            size_t rx_offset;
            size_t rx_length;

            if (qspi_wait_rx_word(&word) != 0) {
                qspi_set_slave_selected(0);
                qspi_write_reg(QSPI_ER, 0U);
                return -1;
            }

            if (received_words == 0U) {
                rx_offset = 0U;
                /* RXD is always a four-byte FIFO word once the command is
                 * longer than four bytes.  The first TX word may contain
                 * only the opcode, but its RX word still contains the
                 * following bus bytes. */
                rx_length = count < 4U ? count : 4U;
            } else {
                rx_offset = (count < 4U ? first_length : 4U) +
                            (received_words - 1U) * 4U;
                rx_length = count - rx_offset;
                if (rx_length > 4U) {
                    rx_length = 4U;
                }
            }
            if (rx != NULL) {
                if (received_words == 0U && count < 4U) {
                    size_t shift = count == 1U ? 24U : 8U;

                    /* For TXD_01/TXD_10/TXD_11 the controller right-aligns
                     * the received bus bytes in the upper part of RXD.
                     * XQspiPs_GetReadData() applies the same correction for
                     * short status-register transactions. */
                    unpack_word(rx, rx_offset, rx_length, word >> shift);
                } else {
                    unpack_word(rx, rx_offset, rx_length, word);
                }
            }
            ++received_words;
        }
    }

    qspi_set_slave_selected(0);
    qspi_write_reg(QSPI_ER, 0U);
    return 0;
}

static int qspi_read_status(uint8_t *status)
{
    uint8_t tx[4] = { QSPI_CMD_READ_STATUS, 0U, 0U, 0U };
    uint8_t rx[4] = { 0U, 0U, 0U, 0U };

    if (status == NULL || qspi_transfer(tx, rx, sizeof(tx)) != 0) {
        return -1;
    }
    *status = rx[1];
    return 0;
}

static int qspi_wait_ready(void)
{
    uint32_t elapsed;

    for (elapsed = 0U; elapsed < QSPI_READY_TIMEOUT_MS; ++elapsed) {
        uint8_t status;

        if (qspi_read_status(&status) != 0) {
            return -1;
        }
        if ((status & QSPI_STATUS_WIP) == 0U) {
            return 0;
        }
        usleep(1000U);
    }

    errno = ETIMEDOUT;
    return -1;
}

static int qspi_write_enable(void)
{
    const uint8_t tx = QSPI_CMD_WRITE_ENABLE;
    uint8_t status;

    if (qspi_transfer(&tx, NULL, 1U) != 0) {
        return -1;
    }
    if (qspi_read_status(&status) != 0) {
        return -1;
    }
    if ((status & QSPI_STATUS_WEL) == 0U) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static int qspi_read_bytes(uint32_t address, uint8_t *data, size_t length)
{
    uint8_t tx[QSPI_TRANSFER_MAX];
    uint8_t rx[QSPI_TRANSFER_MAX];

    if (data == NULL || length == 0U || length > QSPI_IO_CHUNK) {
        errno = EINVAL;
        return -1;
    }

    while (length != 0U) {
        size_t chunk = length > QSPI_TRANSFER_DATA_MAX
                         ? QSPI_TRANSFER_DATA_MAX : length;

        tx[0] = QSPI_CMD_READ_DATA;
        tx[1] = (uint8_t)(address >> 16);
        tx[2] = (uint8_t)(address >> 8);
        tx[3] = (uint8_t)address;
        memset(&tx[QSPI_TRANSFER_HEADER_SIZE], 0xFF, chunk);

        if (qspi_transfer(tx, rx, QSPI_TRANSFER_HEADER_SIZE + chunk) != 0) {
            return -1;
        }
        memcpy(data, &rx[QSPI_TRANSFER_HEADER_SIZE], chunk);
        address += (uint32_t)chunk;
        data += chunk;
        length -= chunk;
    }
    return 0;
}

static int qspi_erase_sector(uint32_t address)
{
    uint8_t tx[QSPI_TRANSFER_HEADER_SIZE] = {
        QSPI_CMD_SECTOR_ERASE_4K,
        (uint8_t)(address >> 16),
        (uint8_t)(address >> 8),
        (uint8_t)address
    };

    if (qspi_write_enable() != 0) {
        return -1;
    }
    if (qspi_transfer(tx, NULL, sizeof(tx)) != 0) {
        return -1;
    }
    if (qspi_wait_ready() != 0) {
        return -1;
    }
    return 0;
}

static int qspi_program_page(uint32_t address, const uint8_t *data,
                             size_t length)
{
    uint8_t tx[QSPI_TRANSFER_MAX];
    size_t offset = 0U;

    if (data == NULL || length == 0U || length > QSPI_FLASH_PAGE_SIZE) {
        errno = EINVAL;
        return -1;
    }

    while (offset < length) {
        size_t chunk = length - offset;
        size_t page_offset = (size_t)((address + offset) %
                                      QSPI_FLASH_PAGE_SIZE);

        if (chunk > QSPI_TRANSFER_DATA_MAX) {
            chunk = QSPI_TRANSFER_DATA_MAX;
        }
        if (chunk > QSPI_FLASH_PAGE_SIZE - page_offset) {
            chunk = QSPI_FLASH_PAGE_SIZE - page_offset;
        }

        tx[0] = QSPI_CMD_PAGE_PROGRAM;
        tx[1] = (uint8_t)((address + offset) >> 16);
        tx[2] = (uint8_t)((address + offset) >> 8);
        tx[3] = (uint8_t)(address + offset);
        memcpy(&tx[QSPI_TRANSFER_HEADER_SIZE], &data[offset], chunk);

        if (qspi_write_enable() != 0) {
            return -1;
        }
        if (qspi_transfer(tx, NULL, QSPI_TRANSFER_HEADER_SIZE + chunk) != 0) {
            return -1;
        }
        if (qspi_wait_ready() != 0) {
            return -1;
        }
        offset += chunk;
    }
    return 0;
}

static int qspi_write_sector(uint32_t address, const uint8_t *data)
{
    uint8_t backup[QSPI_FLASH_SECTOR_SIZE];
    uint32_t offset;

    if ((address % QSPI_FLASH_SECTOR_SIZE) != 0U || data == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (qspi_read_bytes(address, backup, sizeof(backup)) != 0) {
        return -1;
    }
    if (memcmp(backup, data, sizeof(backup)) == 0) {
        return 0;
    }
    if (qspi_erase_sector(address) != 0) {
        return -1;
    }
    for (offset = 0U; offset < QSPI_FLASH_SECTOR_SIZE;
         offset += QSPI_FLASH_PAGE_SIZE) {
        if (qspi_program_page(address + offset, &data[offset],
                              QSPI_FLASH_PAGE_SIZE) != 0) {
            return -1;
        }
    }
    /* A page-program sequence can leave the PS QSPI block in a state where
     * the following read transaction is not reliable.  Reapply the same
     * controller reset/configuration boundary used at startup before the
     * block layer continues with another request. */
    if (qspi_reconfigure() != 0) {
        return -1;
    }
    return 0;
}

static int qspi_read_window(uint64_t offset, uint8_t *data, size_t length)
{
    while (length != 0U) {
        size_t chunk = length > QSPI_IO_CHUNK ? QSPI_IO_CHUNK : length;

        if (qspi_read_bytes((uint32_t)(QSPI_FS_BASE_BYTES + offset),
                            data, chunk) != 0) {
            return -1;
        }
        offset += chunk;
        data += chunk;
        length -= chunk;
    }
    return 0;
}

static int qspi_write_window(uint64_t offset, const uint8_t *data,
                             size_t length)
{
    uint8_t sector[QSPI_FLASH_SECTOR_SIZE];

    while (length != 0U) {
        uint64_t sector_offset = offset & ~(uint64_t)(QSPI_FLASH_SECTOR_SIZE - 1U);
        size_t within = (size_t)(offset - sector_offset);
        size_t chunk = QSPI_IO_BLOCK_SIZE;

        if ((offset % QSPI_IO_BLOCK_SIZE) != 0U ||
            (length % QSPI_IO_BLOCK_SIZE) != 0U) {
            errno = EINVAL;
            return -1;
        }
        if (chunk > length) {
            chunk = length;
        }
        if (chunk > QSPI_FLASH_SECTOR_SIZE - within) {
            chunk = QSPI_FLASH_SECTOR_SIZE - within;
        }
        if (qspi_read_window(sector_offset, sector, sizeof(sector)) != 0) {
            return -1;
        }
        memcpy(&sector[within], data, chunk);
        if (qspi_write_sector(
                (uint32_t)(QSPI_FS_BASE_BYTES + sector_offset),
                              sector) != 0) {
            return -1;
        }
        offset += chunk;
        data += chunk;
        length -= chunk;
    }
    return 0;
}

static int qspi_io_read(resmgr_context_t *ctp, io_read_t *msg,
                        iofunc_ocb_t *ocb)
{
    uint8_t *buffer;
    size_t requested;
    size_t available;
    size_t count;
    uint64_t offset;
    int update_ocb;
    int verify;

    verify = iofunc_read_verify(ctp, msg, ocb, NULL);
    if (verify != EOK) {
        return verify;
    }

    switch (msg->i.xtype & _IO_XTYPE_MASK) {
    case _IO_XTYPE_NONE:
        if (ocb->offset < 0) {
            return _RESMGR_ERRNO(ENOSPC);
        }
        offset = (uint64_t)ocb->offset;
        update_ocb = 1;
        break;
    case _IO_XTYPE_OFFSET:
        if (((bvstk_qspi_read_offset_t *)msg)->offset.offset < 0) {
            return _RESMGR_ERRNO(ENOSPC);
        }
        offset = (uint64_t)((bvstk_qspi_read_offset_t *)msg)->offset.offset;
        update_ocb = 0;
        break;
    default:
        return _RESMGR_ERRNO(ENOSYS);
    }

    if (offset > QSPI_FS_SIZE_BYTES) {
        return _RESMGR_ERRNO(ENOSPC);
    }
    if (offset == QSPI_FS_SIZE_BYTES) {
        return _IO_SET_READ_NBYTES(ctp, 0);
    }

    requested = (size_t)msg->i.nbytes;
    available = QSPI_FS_SIZE_BYTES - (size_t)offset;
    count = requested < available ? requested : available;
    if (count == 0U) {
        return _IO_SET_READ_NBYTES(ctp, 0);
    }

    /* devb-loopback may issue one pread() for many blocks (for example
     * 16--24 KiB).  Returning only QSPI_IO_CHUNK bytes makes the block layer
     * retry the same _IO_XTYPE_OFFSET request indefinitely. */
    if (count > (size_t)INT_MAX) {
        return _RESMGR_ERRNO(EOVERFLOW);
    }
    buffer = malloc(count);
    if (buffer == NULL) {
        return _RESMGR_ERRNO(ENOMEM);
    }

    {
        int lock_result = pthread_mutex_lock(&g_qspi.lock);
        int io_error = EOK;

        if (lock_result != 0) {
            free(buffer);
            return _RESMGR_ERRNO(lock_result);
        }
        if (qspi_read_window(offset, buffer, count) == 0) {
            _IO_SET_READ_NBYTES(ctp, count);
            if (resmgr_msgwrite(ctp, buffer, (int)count, 0) !=
                (int)count) {
                io_error = EIO;
            }
        } else {
            io_error = errno == 0 ? EIO : errno;
        }
        (void)pthread_mutex_unlock(&g_qspi.lock);
        free(buffer);
        if (io_error != EOK) {
            return _RESMGR_ERRNO(io_error);
        }
    }

    if (update_ocb) {
        ocb->offset += (off_t)count;
    }
    return EOK;
}

static int qspi_io_write(resmgr_context_t *ctp, io_write_t *msg,
                         iofunc_ocb_t *ocb)
{
    uint8_t buffer[QSPI_IO_CHUNK];
    size_t requested;
    size_t available;
    size_t count;
    size_t processed = 0U;
    uint64_t offset;
    size_t message_data_offset;
    int update_ocb;
    int verify;

    verify = iofunc_write_verify(ctp, msg, ocb, NULL);
    if (verify != EOK) {
        return verify;
    }
    if ((ocb->ioflag & O_ACCMODE) == O_RDONLY) {
        return _RESMGR_ERRNO(EBADF);
    }

    switch (msg->i.xtype & _IO_XTYPE_MASK) {
    case _IO_XTYPE_NONE:
        if (ocb->offset < 0) {
            return _RESMGR_ERRNO(ENOSPC);
        }
        offset = (uint64_t)ocb->offset;
        message_data_offset = sizeof(msg->i);
        update_ocb = 1;
        break;
    case _IO_XTYPE_OFFSET:
        if (((bvstk_qspi_write_offset_t *)msg)->offset.offset < 0) {
            return _RESMGR_ERRNO(ENOSPC);
        }
        offset = (uint64_t)((bvstk_qspi_write_offset_t *)msg)->offset.offset;
        message_data_offset = sizeof(msg->i) + sizeof(struct _xtype_offset);
        update_ocb = 0;
        break;
    default:
        return _RESMGR_ERRNO(ENOSYS);
    }

    if (offset >= QSPI_FS_SIZE_BYTES) {
        return _RESMGR_ERRNO(ENOSPC);
    }

    requested = (size_t)msg->i.nbytes;
    available = QSPI_FS_SIZE_BYTES - (size_t)offset;
    count = requested < available ? requested : available;
    if (count == 0U || (count % QSPI_IO_BLOCK_SIZE) != 0U ||
        ((size_t)offset % QSPI_IO_BLOCK_SIZE) != 0U) {
        return _RESMGR_ERRNO(EINVAL);
    }

    {
        int lock_result = pthread_mutex_lock(&g_qspi.lock);
        int io_error = EOK;

        if (lock_result != 0) {
            return _RESMGR_ERRNO(lock_result);
        }
        while (processed < count) {
            size_t chunk = count - processed;

            if (chunk > sizeof(buffer)) {
                chunk = sizeof(buffer);
            }
            if (resmgr_msgread(ctp, buffer, (int)chunk,
                               (int)message_data_offset + (int)processed) !=
                (int)chunk) {
                io_error = errno == 0 ? EIO : errno;
                break;
            }
            if (qspi_write_window(offset + processed, buffer, chunk) != 0) {
                io_error = errno == 0 ? EIO : errno;
                break;
            }
            processed += chunk;
        }
        (void)pthread_mutex_unlock(&g_qspi.lock);
        if (io_error != EOK && processed == 0U) {
            return _RESMGR_ERRNO(io_error);
        }
    }

    if (update_ocb) {
        ocb->offset += (off_t)processed;
    }
    _IO_SET_WRITE_NBYTES(ctp, processed);
    return EOK;
}

static int qspi_backend_init(void)
{
    uint32_t config;
    void *mapped;
    uint8_t tx[4] = { QSPI_CMD_READ_ID, 0U, 0U, 0U };
    uint8_t rx[4] = { 0U, 0U, 0U, 0U };

    if (ThreadCtl(_NTO_TCTL_IO, NULL) == -1) {
        return -1;
    }
    mapped = mmap_device_memory(NULL,
                                BVSTK_QSPI_MAP_SIZE,
                                PROT_READ | PROT_WRITE | PROT_NOCACHE,
                                0,
                                BVSTK_QSPI_BASE);
    if (mapped == MAP_FAILED) {
        return -1;
    }
    memset(&g_qspi, 0, sizeof(g_qspi));
    g_qspi.base = (volatile uint8_t *)mapped;
    if (pthread_mutex_init(&g_qspi.lock, NULL) != 0) {
        (void)munmap(mapped, BVSTK_QSPI_MAP_SIZE);
        errno = EBUSY;
        return -1;
    }

    config = QSPI_CR_IFMODE | QSPI_CR_MSTREN | QSPI_CR_DATA_SZ |
             QSPI_CR_SSFORCE | QSPI_CR_HOLD_B | QSPI_CR_SSCTRL |
             (UINT32_C(2) << QSPI_CR_PRESC_SHIFT);
    g_qspi_config = config;
    if (qspi_reconfigure() != 0) {
        fprintf(stderr, "bvstk-qspi-fat: controller reset failed: %s\n",
                strerror(errno));
        return -1;
    }

    if (qspi_transfer(tx, rx, sizeof(tx)) != 0) {
        fprintf(stderr, "bvstk-qspi-fat: JEDEC read failed: %s\n",
                strerror(errno));
        return -1;
    }
    fprintf(stdout,
            "bvstk-qspi-fat: JEDEC %02x %02x %02x, window=0x%08x..0x%08x\n",
            rx[1], rx[2], rx[3], QSPI_FS_BASE_BYTES,
            QSPI_FS_BASE_BYTES + QSPI_FS_SIZE_BYTES - 1U);
    return 0;
}

int main(void)
{
    dispatch_t *dispatch;
    dispatch_context_t *context;
    resmgr_attr_t resmgr_attr;
    resmgr_connect_funcs_t connect_funcs;
    resmgr_io_funcs_t io_funcs;
    int attach_id;

    if (qspi_backend_init() != 0) {
        fprintf(stderr, "bvstk-qspi-fat: initialization failed: %s\n",
                strerror(errno));
        return EXIT_FAILURE;
    }

    memset(&g_qspi.attr, 0, sizeof(g_qspi.attr));
    iofunc_attr_init(&g_qspi.attr, S_IFREG | 0660, NULL, NULL);
    g_qspi.attr.nbytes = QSPI_FS_SIZE_BYTES;

    dispatch = dispatch_create();
    if (dispatch == NULL) {
        fprintf(stderr, "bvstk-qspi-fat: dispatch_create failed: %s\n",
                strerror(errno));
        return EXIT_FAILURE;
    }
    memset(&resmgr_attr, 0, sizeof(resmgr_attr));
    resmgr_attr.nparts_max = 1U;
    resmgr_attr.msg_max_size = 8192U;
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs,
                     _RESMGR_IO_NFUNCS, &io_funcs);
    io_funcs.read = qspi_io_read;
    io_funcs.write = qspi_io_write;

    attach_id = resmgr_attach(dispatch, &resmgr_attr,
                               "/dev/bvstk-qspi-fat", _FTYPE_ANY, 0U,
                               &connect_funcs, &io_funcs, &g_qspi.attr);
    if (attach_id == -1) {
        fprintf(stderr, "bvstk-qspi-fat: resmgr_attach failed: %s\n",
                strerror(errno));
        return EXIT_FAILURE;
    }

    fprintf(stdout, "bvstk-qspi-fat: ready at /dev/bvstk-qspi-fat\n");
    context = dispatch_context_alloc(dispatch);
    if (context == NULL) {
        fprintf(stderr, "bvstk-qspi-fat: dispatch_context_alloc failed: %s\n",
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
