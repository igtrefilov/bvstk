#include "apps/freertos/console/sd_pl_shell.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.h"
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.h"
#include "apps/freertos/storage/sd-pl/sd_pl_card.h"

static void output(int fd, const char *format, ...)
{
    char buffer[256];
    va_list args;
    int length;
    va_start(args, format);
    length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (length <= 0) return;
    if (length >= (int)sizeof(buffer)) length = (int)sizeof(buffer) - 1;
    (void)console_stream_write(fd, buffer, (size_t)length);
}

static bool number(const char *text, uint32_t *value)
{
    char *end;
    unsigned long parsed;
    if (!text || !*text || *text == '-' || *text == '+') return false;
    errno = 0;
    parsed = strtoul(text, &end, text[0] == '0' &&
        (text[1] == 'x' || text[1] == 'X') ? 16 : 10);
    if (errno || *end || parsed > UINT32_MAX) return false;
    *value = (uint32_t)parsed;
    return true;
}

static void hardware_report(int fd, const bvstk_sd_master_report_t *h)
{
    output(fd, "sd-pl: CMD%u stage=%s time=%lu ms reply=%08lX first=%08lX\r\n",
        h->command, h->stage ? h->stage : "none",
        (unsigned long)h->elapsed_ms, (unsigned long)h->reply, (unsigned long)h->first_reply);
    output(fd, "sd-pl: csr=%08lX irq=%08lX irq_count=%lu tx=%08lX rx=%08lX rx_len=%lu\r\n",
        (unsigned long)h->csr, (unsigned long)h->irq, (unsigned long)h->irq_count,
        (unsigned long)h->tx_status, (unsigned long)h->rx_status,
        (unsigned long)h->rx_length);
    if (h->command == 17)
        output(fd, "sd-pl: CRC16 received=%04X calculated=%04X guards=%u\r\n",
            h->crc_received, h->crc_calculated, (unsigned)h->guards_ok);
    if (h->read_idle_reply)
        write_str(fd, "sd-pl: CMD17 mailbox=idle sample, not latched R1; full data CRC verified\r\n");
}

static void result_report(int fd, const bvstk_sd_result_t *r)
{
    output(fd, "sd-pl: LBA=%lu result=%s\r\n", (unsigned long)r->lba,
        bvstk_status_string(r->status));
    if (r->reinitializations) {
        output(fd, "sd-pl: isolated profile, preparation reinitializations=%lu\r\n",
            (unsigned long)r->reinitializations);
        hardware_report(fd, &r->prepare_hardware);
    }
    if (r->write_hardware.stage && r->hardware.command != 24)
        hardware_report(fd, &r->write_hardware);
    hardware_report(fd, &r->hardware);
    if (r->mismatch_offset != UINT32_MAX)
        output(fd, "sd-pl: mismatch LBA=%lu byte=%lu\r\n",
            (unsigned long)r->lba, (unsigned long)r->mismatch_offset);
    if (r->status != BVSTK_OK)
        output(fd, "ERR: sd-pl %s; inspect report; filesystem mode requires restart after I/O errors\r\n",
            bvstk_status_string(r->status));
}

bool sd_pl_handle(char *tok, char **save, int fd)
{
    char *sub, *args[4];
    size_t n = 0, i;
    uint32_t values[3] = {0, 2, UINT32_C(0x53445453)};
    uint8_t block[SDM_BLOCK_SIZE];
    bvstk_sd_result_t result;
    bvstk_sd_card_info_t info;
    bvstk_status_t status;
    bvstk_sd_service_t *service;
    if (!tok || strcasecmp(tok, "sd-pl") != 0) return false;
    sub = strtok_r(NULL, " \t", save);
    if (!sub || !strcasecmp(sub, "-h") || !strcasecmp(sub, "--help") ||
        !strcasecmp(sub, "help")) { sd_pl_help(fd); return true; }
    while (n < 4 && (args[n] = strtok_r(NULL, " \t", save)) != NULL) ++n;
    if (n > 3) goto usage;
    for (i = 0; i < n; ++i) if (!number(args[i], &values[i])) goto usage;
    service = bvstk_sd_master_freertos_service();
    if (!service) {
        write_str(fd, "ERR: sd-pl service unavailable; start its FreeRTOS port before console clients\r\n");
        return true;
    }
    if (!strcasecmp(sub, "info") && n == 0) {
        status = bvstk_sd_service_info(service, &info);
        if (status != BVSTK_OK) { output(fd, "ERR: %s\r\n", bvstk_status_string(status)); return true; }
        output(fd, "sd-pl: core=%08lX dma=%08lX bram=%08lX divider=%u\r\n",
            (unsigned long)service->driver->config.core_base,
            (unsigned long)service->driver->config.dma_base,
            (unsigned long)service->driver->config.bram_base,
            service->driver->config.data_divider);
        output(fd, "sd-pl: ready=%u OCR=%08lX addressing=%s block=512 capacity=unknown\r\n",
            (unsigned)info.ready, (unsigned long)info.ocr,
            info.ocr == 0 ? "unknown" : (info.block_addressed ? "LBA" : "byte"));
        output(fd, "sd-pl: CMD17 descriptor=%u bytes isolated_commands=%u\r\n",
            service->driver->config.read_descriptor_bytes,
            (unsigned)service->reinitialize_between_commands);
        output(fd, "sd-pl: sd-pl:/ mounted=%u filesystem_owned=%u format=disabled\r\n",
            (unsigned)sd_pl_card_is_ready(), (unsigned)info.filesystem_owned);
        const bvstk_sd_volume_t *volume = bvstk_sd_pl_volume();
        if (volume) output(fd, "sd-pl: FAT%u volume LBA=%lu sectors=%lu (not card capacity)\r\n",
            volume->fat_bits, (unsigned long)volume->first_lba, (unsigned long)volume->sectors);
        if (info.last.status == BVSTK_ERR_NOT_READY)
            write_str(fd, "sd-pl: card not initialized; check boot log\r\n");
        else result_report(fd, &info.last);
        return true;
    }
    status = bvstk_sd_service_info(service, &info);
    if (status != BVSTK_OK || info.filesystem_owned) {
        write_str(fd, "ERR: raw commands disabled while filesystem owns card; use ls/cat/fs write sd-pl:/...\r\n");
        return true;
    }
    if (!strcasecmp(sub, "init") && n <= 1) {
        if (n && (values[0] == 0 || values[0] > 120000)) goto usage;
        write_str(fd, "sd-pl: initializing external card...\r\n");
        status = bvstk_sd_service_initialize(service, values[0], &result);
        for (i = 0; i < SDM_INIT_WORDS; ++i)
            output(fd, "sd-pl: init[%u]=%08lX\r\n", (unsigned)i,
                (unsigned long)result.hardware.init_reply[i]);
    } else if (!strcasecmp(sub, "status") && n == 0) {
        status = bvstk_sd_service_card_status(service, &result);
    } else if (!strcasecmp(sub, "blocklen") && n == 0) {
        status = bvstk_sd_service_block_length(service, &result);
    } else if (!strcasecmp(sub, "read") && n == 1) {
        status = bvstk_sd_service_read(service, values[0], block, &result);
        if (status == BVSTK_OK) {
            for (i = 0; i < sizeof(block); i += 16) {
                size_t j;
                output(fd, "%03X:", (unsigned)i);
                for (j = 0; j < 16; ++j) output(fd, " %02X", block[i + j]);
                write_str(fd, "\r\n");
            }
        }
    } else if (!strcasecmp(sub, "write") && n == 2) {
        output(fd, "sd-pl: OVERWRITE LBA=%lu seed=%08lX; then verify all 512 bytes\r\n",
            (unsigned long)values[0], (unsigned long)values[1]);
        bvstk_sd_service_pattern(block, values[0], values[1]);
        status = bvstk_sd_service_write_verify(service, values[0], block, &result);
    } else if (!strcasecmp(sub, "test") && n >= 1 && n <= 3) {
        if (values[1] == 0 || values[1] > 8 || values[0] > UINT32_MAX - (values[1] - 1)) goto usage;
        output(fd, "sd-pl: OVERWRITE LBA=%lu..%lu seed=%08lX; write range, then verify\r\n",
            (unsigned long)values[0], (unsigned long)(values[0] + values[1] - 1),
            (unsigned long)values[2]);
        status = bvstk_sd_service_test(service, values[0], values[1], values[2], &result);
    } else goto usage;
    result_report(fd, &result);
    if (status == BVSTK_OK) output(fd, "OK: sd-pl %s\r\n", sub);
    return true;
usage:
    write_str(fd, "ERR: invalid sd-pl command/arguments (unsigned decimal or 0xHEX)\r\n");
    sd_pl_help(fd);
    return true;
}

void sd_pl_help(int fd)
{
    write_str(fd, "sd-pl: auto-mounted existing FAT volume at sd-pl:/; formatting disabled\r\n");
    write_str(fd, "Use ls/cd/mkdir/cat/cp/mv/rm and fs write|append with sd-pl:/ paths.\r\n");
    write_str(fd, "Raw diagnostics below are disabled when the filesystem owns the card:\r\n");
    write_str(fd, "  sd-pl init [timeout_ms]         1..120000, default 20000\r\n");
    write_str(fd, "  sd-pl info                      cached state and last report\r\n");
    write_str(fd, "  sd-pl status                    CMD13, card status\r\n");
    write_str(fd, "  sd-pl blocklen                  explicit CMD16(512) diagnostic\r\n");
    write_str(fd, "  sd-pl read <lba>                 dump one sector, check CRC16\r\n");
    write_str(fd, "  sd-pl write <lba> <seed>         OVERWRITE with pattern + readback\r\n");
    write_str(fd, "  sd-pl test <lba> [count] [seed]  OVERWRITE 1..8 sectors, default 2\r\n");
    write_str(fd, "WARNING: raw write/test destroy data. Supply an existing expendable LBA.\r\n");
    write_str(fd, "No physical capacity discovery, erase, or retry of failed media writes.\r\n");
    write_str(fd, "AX7020 profile: SDHC/SDXC; reinitialize before each subsequent command.\r\n");
}
