#include "apps/freertos/console/sd_pl_shell.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "drivers/pl/sd/bvstk_sd_init.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "ports/freertos-xilinx/os/bvstk_sync_freertos.h"

static bvstk_sd_init_t s_driver;
static bvstk_freertos_mutex_t s_mutex;
static bool s_mutex_ready;
static bool s_driver_ready;

static void sd_pl_writef(int fd, const char *format, ...)
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

static bool sd_pl_driver_ready(void)
{
#if !BVSTK_PL_SD_CAN_INIT
    return false;
#else
    if (!s_mutex_ready) {
        if (bvstk_freertos_mutex_init(&s_mutex) != BVSTK_OK) {
            return false;
        }
        s_mutex_ready = true;
    }
    if (!s_driver_ready) {
        if (bvstk_sd_init_init(&s_driver,
                               NULL,
                               &s_mutex.public_mutex) != BVSTK_OK) {
            bvstk_freertos_mutex_destroy(&s_mutex);
            s_mutex_ready = false;
            return false;
        }
        s_driver_ready = true;
    }
    return true;
#endif
}

static void cmd_init(int fd, char **save)
{
    bvstk_sd_init_result_t result;
    bvstk_status_t status;
    char *timeout_arg;
    unsigned long timeout = 20000UL;

    timeout_arg = strtok_r(NULL, " \t", save);
    if (timeout_arg != NULL) {
        char *end = NULL;
        timeout = strtoul(timeout_arg, &end, 0);
        if (end == NULL || *end != '\0' || timeout == 0UL || timeout > 120000UL) {
            write_str(fd, "ERR: timeout must be 1..120000 ms\r\n");
            return;
        }
    }
    if (strtok_r(NULL, " \t", save) != NULL) {
        write_str(fd, "ERR: too many arguments\r\n");
        return;
    }
    if (!sd_pl_driver_ready()) {
        write_str(fd, "ERR: sd-pl initialization sequencer unavailable\r\n");
        return;
    }

    sd_pl_writef(fd,
                 "sd-pl: init start ctrl=0x%08lX bram=0x%08lX\r\n",
                 (unsigned long)BVSTK_SD_CONTROLLER_BASE,
                 (unsigned long)BVSTK_SPI_BRAM_BASE);
    status = bvstk_sd_init_run(&s_driver, (uint32_t)timeout, &result);
    sd_pl_writef(fd,
                 "sd-pl: irq=0x%08lX hard=%u soft=%u csr=0x%08lX\r\n",
                 (unsigned long)result.irq,
                 (unsigned)result.hard_done,
                 (unsigned)result.soft_done,
                 (unsigned long)result.csr);
    sd_pl_writef(fd, "sd-pl: CMD0       0x%08lX\r\n",
                 (unsigned long)result.response[0]);
    sd_pl_writef(fd, "sd-pl: CMD8[0]    0x%08lX\r\n",
                 (unsigned long)result.response[1]);
    sd_pl_writef(fd, "sd-pl: CMD8[1]    0x%08lX\r\n",
                 (unsigned long)result.response[2]);
    sd_pl_writef(fd, "sd-pl: ACMD41     0x%08lX\r\n",
                 (unsigned long)result.response[3]);
    sd_pl_writef(fd, "sd-pl: CMD55      0x%08lX\r\n",
                 (unsigned long)result.response[4]);
    sd_pl_writef(fd, "sd-pl: CMD58[0]   0x%08lX\r\n",
                 (unsigned long)result.response[5]);
    sd_pl_writef(fd, "sd-pl: CMD58[1]   0x%08lX\r\n",
                 (unsigned long)result.response[6]);
    if (status != BVSTK_OK) {
        sd_pl_writef(fd, "ERR: sd-pl init %s (%d)\r\n",
                     bvstk_status_string(status), (int)status);
        return;
    }
    write_str(fd, "OK: sd-pl card initialization completed\r\n");
}

bool sd_pl_handle(char *tok, char **save, int fd)
{
    char *sub;

    if (tok == NULL || strcasecmp(tok, "sd-pl") != 0) return false;
    sub = strtok_r(NULL, " \t", save);
    if (sub == NULL || strcasecmp(sub, "-h") == 0 ||
        strcasecmp(sub, "--help") == 0 || strcasecmp(sub, "help") == 0) {
        sd_pl_help(fd);
        return true;
    }
    if (strcasecmp(sub, "init") == 0) {
        cmd_init(fd, save);
        return true;
    }
    write_str(fd, "ERR: unknown sd-pl command\r\n");
    return true;
}

void sd_pl_help(int fd)
{
    write_str(fd, "sd-pl usage:\r\n");
    write_str(fd, "  sd-pl init [timeout_ms]\r\n");
    write_str(fd, "notes:\r\n");
    write_str(fd, "  current PL contract exposes card initialization only\r\n");
    write_str(fd, "  sd-pl filesystem/block commands are reserved for the next contract\r\n");
}
