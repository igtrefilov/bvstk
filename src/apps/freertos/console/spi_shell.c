#include "apps/freertos/console/spi_shell.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "apps/freertos/runtime/bvstk_runtime.h"
#include "apps/freertos/services/dcp2/dcp2_stream_sim.h"
#include "apps/freertos/console/stream_shell.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "hardware/pl/spi/bvstk_spi_regs.h"

#if BVSTK_PL_HAS_SPI_CORE
static void spi_writef(int fd, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
    (void)console_stream_write(fd, buf, (size_t)n);
}
#endif

static void cmd_info(int fd)
{
#if !BVSTK_PL_HAS_SPI_CORE
    write_str(fd, "ERR (SPI not available)\r\n");
    return;
#else
    bvstk_spi_core_t *core = bvstk_runtime_spi_core();
    bvstk_spi_core_config_t c;
    if (core == NULL || bvstk_spi_core_get_config(core, &c) != BVSTK_OK) {
        write_str(fd, "ERR not-ready\r\n");
        return;
    }
    const char *mode = (c.packets_mode == BVSTK_SPI_MODE_SINGLE) ? "single" :
                       (c.packets_mode == BVSTK_SPI_MODE_FALLTHROUGH) ? "fallthrough" : "multi";
    spi_writef(fd, "spi: mode=%s timeout=%lu p_clk_div=%u read=%s\r\n",
               mode,
               (unsigned long)c.timeout_ticks,
               (unsigned)c.p_clk_div,
               c.read_enable ? "on" : "off");
#endif
}

static void cmd_cfg(int fd, const char *k, const char *v)
{
    if (!k || !v) { write_str(fd, "ERR\r\n"); return; }
#if !BVSTK_PL_HAS_SPI_CORE
    write_str(fd, "ERR (SPI not available)\r\n");
    return;
#else

    bvstk_spi_core_t *core = bvstk_runtime_spi_core();
    bvstk_spi_core_config_t c;
    if (core == NULL || bvstk_spi_core_get_config(core, &c) != BVSTK_OK) {
        write_str(fd, "ERR not-ready\r\n");
        return;
    }

    if (strcasecmp(k, "mode") == 0) {
        if (strcasecmp(v, "single") == 0) c.packets_mode = BVSTK_SPI_MODE_SINGLE;
        else if (strcasecmp(v, "multi") == 0) c.packets_mode = BVSTK_SPI_MODE_MULTI;
        else if (strcasecmp(v, "fall") == 0 || strcasecmp(v, "fallthrough") == 0) c.packets_mode = BVSTK_SPI_MODE_FALLTHROUGH;
        else { write_str(fd, "ERR\r\n"); return; }
    } else if (strcasecmp(k, "timeout") == 0) {
        bool ok = false;
        unsigned long t = parse_num(v, &ok);
        if (!ok || t == 0 || t > 0xFFFFFFFFul) { write_str(fd, "ERR\r\n"); return; }
        c.timeout_ticks = (uint32_t)t;
    } else if (strcasecmp(k, "div") == 0 || strcasecmp(k, "p_clk_div") == 0) {
        bool ok = false;
        unsigned long d = parse_num(v, &ok);
        if (!ok || d < 2 || d > 0xFFFFul) { write_str(fd, "ERR\r\n"); return; }
        c.p_clk_div = (uint16_t)d;
    } else if (strcasecmp(k, "read") == 0) {
        if (strcasecmp(v, "on") == 0 || strcmp(v, "1") == 0) c.read_enable = true;
        else if (strcasecmp(v, "off") == 0 || strcmp(v, "0") == 0) c.read_enable = false;
        else { write_str(fd, "ERR\r\n"); return; }
    } else {
        write_str(fd, "ERR\r\n");
        return;
    }

    if (bvstk_spi_core_set_config(core, &c) != BVSTK_OK) {
        write_str(fd, "ERR\r\n");
        return;
    }
    write_str(fd, "OK\r\n");
#endif
}

static void cmd_xfer(int fd, char **save)
{
#if !BVSTK_PL_HAS_SPI_CORE
    (void)save;
    write_str(fd, "ERR (SPI not available)\r\n");
    return;
#else
    uint32_t tx[64];
    uint32_t rx[64];
    size_t n = 0;

    for (;;) {
        char *a = strtok_r(NULL, " \t", save);
        if (!a) break;
        if (n >= 64) { write_str(fd, "ERR too many words\r\n"); return; }
        bool ok = false;
        unsigned long v = parse_num(a, &ok);
        if (!ok || v > 0xFFFFFFFFul) { write_str(fd, "ERR\r\n"); return; }
        tx[n++] = (uint32_t)v;
    }

    if (n == 0) { write_str(fd, "ERR\r\n"); return; }

    bvstk_spi_core_t *core = bvstk_runtime_spi_core();
    if (core == NULL) {
        write_str(fd, "ERR not-ready\r\n");
        return;
    }
    if (bvstk_spi_core_transfer(core, tx, n, rx, n, 250U) != BVSTK_OK) {
        write_str(fd, "ERR timeout\r\n");
        return;
    }

    for (size_t i = 0; i < n; ++i) {
        spi_writef(fd, "RX[%u]=0x%08lX\r\n", (unsigned)i, (unsigned long)rx[i]);
    }
    write_str(fd, "OK\r\n");
#endif
}

bool spi_handle(char *tok, char **save, int fd)
{
    if (!tok || strcasecmp(tok, "spi") != 0) return false;

    char *sub = strtok_r(NULL, " \t", save);
    if (!sub || strcasecmp(sub, "-h") == 0 || strcasecmp(sub, "--help") == 0) {
        spi_help(fd);
        return true;
    }
    if (strcasecmp(sub, "stream") == 0) {
        (void)dcp2_stream_shell_handle(DCP2_STREAM_SERVICE_SPI, save, fd);
        return true;
    }

    if (strcasecmp(sub, "info") == 0) { cmd_info(fd); return true; }
    if (strcasecmp(sub, "cfg") == 0) {
        cmd_cfg(fd, strtok_r(NULL, " \t", save), strtok_r(NULL, " \t", save));
        return true;
    }
    if (strcasecmp(sub, "xfer") == 0 || strcasecmp(sub, "tx") == 0) {
        cmd_xfer(fd, save);
        return true;
    }

    write_str(fd, "ERR\r\n");
    return true;
}

void spi_help(int fd)
{
    write_str(fd, "spi usage:\r\n");
    write_str(fd, "  spi stream fake enable [period_ms] [counter|ramp|toggle] [words]\r\n");
    write_str(fd, "  spi stream fake disable|status\r\n");
    write_str(fd, "  spi info\r\n");
    write_str(fd, "  spi cfg mode <single|multi|fallthrough>\r\n");
    write_str(fd, "  spi cfg timeout <ticks>\r\n");
    write_str(fd, "  spi cfg div <even>=2..65535\r\n");
    write_str(fd, "  spi cfg read <on|off>\r\n");
    write_str(fd, "  spi xfer <w0> [w1 ...]    (32-bit words, hex or dec)\r\n");
}
