#include "apps/freertos/console/stream_shell.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "apps/freertos/console/console_common.h"
#include "apps/freertos/services/dcp2/dcp2_stream_sim.h"

static void stream_writef(int fd, const char *format, ...)
{
    char buffer[256];
    va_list ap;
    int length;

    va_start(ap, format);
    length = vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);
    if (length <= 0) {
        return;
    }
    if (length >= (int)sizeof(buffer)) {
        length = (int)sizeof(buffer) - 1;
    }
    (void)console_stream_write(fd, buffer, (size_t)length);
}

static bool parse_period(const char *text, uint32_t *period_ms)
{
    bool ok = false;
    unsigned long value;

    if (text == NULL || period_ms == NULL) {
        return false;
    }
    value = parse_num(text, &ok);
    if (!ok || value < DCP2_STREAM_SIM_MIN_PERIOD_MS ||
        value > DCP2_STREAM_SIM_MAX_PERIOD_MS) {
        return false;
    }
    *period_ms = (uint32_t)value;
    return true;
}

static bool parse_words(const char *text, uint16_t *words)
{
    bool ok = false;
    unsigned long value;

    if (text == NULL || words == NULL) {
        return false;
    }
    value = parse_num(text, &ok);
    if (!ok || value == 0UL || value > DCP2_STREAM_SIM_MAX_WORDS) {
        return false;
    }
    *words = (uint16_t)value;
    return true;
}

static bool parse_pattern(const char *text,
                          dcp2_stream_sim_pattern_t *pattern)
{
    if (text == NULL || pattern == NULL) {
        return false;
    }
    if (strcasecmp(text, "counter") == 0) {
        *pattern = DCP2_STREAM_SIM_PATTERN_COUNTER;
        return true;
    }
    if (strcasecmp(text, "ramp") == 0) {
        *pattern = DCP2_STREAM_SIM_PATTERN_RAMP;
        return true;
    }
    if (strcasecmp(text, "toggle") == 0) {
        *pattern = DCP2_STREAM_SIM_PATTERN_TOGGLE;
        return true;
    }
    return false;
}

static void print_status(int fd, uint8_t service)
{
    dcp2_stream_sim_status_t status;

    if (!dcp2_stream_sim_get_status(service, &status)) {
        write_str(fd, "ERR (unknown stream service)\r\n");
        return;
    }
    stream_writef(fd,
                  "%s stream fake: %s period_ms=%lu pattern=%s words=%u "
                  "emitted=%lu lost=%lu\r\n",
                  dcp2_stream_sim_service_name(service),
                  status.enabled ? "enabled" : "disabled",
                  (unsigned long)status.period_ms,
                  dcp2_stream_sim_pattern_name(status.pattern),
                  (unsigned)status.words,
                  (unsigned long)status.emitted_count,
                  (unsigned long)status.lost_count);
}

void dcp2_stream_shell_help(uint8_t service, int fd)
{
    const char *name = dcp2_stream_sim_service_name(service);

    stream_writef(fd, "%s stream usage:\r\n", name);
    stream_writef(fd,
                  "  %s stream fake enable [period_ms] "
                  "[counter|ramp|toggle] [words]\r\n",
                  name);
    stream_writef(fd, "  %s stream fake disable\r\n", name);
    stream_writef(fd, "  %s stream fake status\r\n", name);
    stream_writef(fd, "  %s stream status\r\n", name);
    write_str(fd,
              "  defaults: period_ms=100, pattern=counter, words=4; "
              "period range=10..60000 ms, words range=1..64\r\n");
}

bool dcp2_stream_shell_handle(uint8_t service, char **save, int fd)
{
    char *mode = strtok_r(NULL, " \t", save);
    char *action;

    if (mode == NULL || strcasecmp(mode, "-h") == 0 ||
        strcasecmp(mode, "--help") == 0) {
        dcp2_stream_shell_help(service, fd);
        return true;
    }

    if (strcasecmp(mode, "status") == 0) {
        print_status(fd, service);
        return true;
    }

    if (strcasecmp(mode, "fake") != 0) {
        if (strcasecmp(mode, "enable") == 0 ||
            strcasecmp(mode, "disable") == 0) {
            write_str(fd,
                      "ERR (real stream source unavailable; use fake stream)\r\n");
        } else {
            write_str(fd, "ERR (expected fake or status)\r\n");
        }
        return true;
    }

    action = strtok_r(NULL, " \t", save);
    if (action == NULL || strcasecmp(action, "-h") == 0 ||
        strcasecmp(action, "--help") == 0) {
        dcp2_stream_shell_help(service, fd);
        return true;
    }
    if (strcasecmp(action, "status") == 0) {
        print_status(fd, service);
        return true;
    }
    if (strcasecmp(action, "disable") == 0 ||
        strcasecmp(action, "off") == 0) {
        if (!dcp2_stream_sim_configure(service,
                                       false,
                                       DCP2_STREAM_SIM_DEFAULT_PERIOD_MS,
                                       DCP2_STREAM_SIM_PATTERN_COUNTER,
                                       DCP2_STREAM_SIM_DEFAULT_WORDS)) {
            write_str(fd, "ERR (failed to disable fake stream)\r\n");
            return true;
        }
        stream_writef(fd,
                      "OK %s stream fake disabled\r\n",
                      dcp2_stream_sim_service_name(service));
        return true;
    }
    if (strcasecmp(action, "enable") != 0 &&
        strcasecmp(action, "on") != 0) {
        write_str(fd, "ERR (expected enable, disable or status)\r\n");
        return true;
    }

    {
        char *period_text = strtok_r(NULL, " \t", save);
        char *pattern_text = strtok_r(NULL, " \t", save);
        char *words_text = strtok_r(NULL, " \t", save);
        uint32_t period_ms = DCP2_STREAM_SIM_DEFAULT_PERIOD_MS;
        dcp2_stream_sim_pattern_t pattern = DCP2_STREAM_SIM_PATTERN_COUNTER;
        uint16_t words = DCP2_STREAM_SIM_DEFAULT_WORDS;

        if (period_text != NULL && !parse_period(period_text, &period_ms)) {
            write_str(fd, "ERR (period_ms must be 10..60000)\r\n");
            return true;
        }
        if (pattern_text != NULL && !parse_pattern(pattern_text, &pattern)) {
            write_str(fd, "ERR (pattern must be counter, ramp or toggle)\r\n");
            return true;
        }
        if (words_text != NULL && !parse_words(words_text, &words)) {
            write_str(fd, "ERR (words must be 1..64)\r\n");
            return true;
        }
        if (!dcp2_stream_sim_configure(service,
                                       true,
                                       period_ms,
                                       pattern,
                                       words)) {
            write_str(fd, "ERR (failed to enable fake stream)\r\n");
            return true;
        }
        stream_writef(fd,
                      "OK %s stream fake enabled period_ms=%lu pattern=%s "
                      "words=%u\r\n",
                      dcp2_stream_sim_service_name(service),
                      (unsigned long)period_ms,
                      dcp2_stream_sim_pattern_name(pattern),
                      (unsigned)words);
    }
    return true;
}
