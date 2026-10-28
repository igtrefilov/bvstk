#include "services/cli/bvstk_cli.h"
#include "shared/base/bvstk_parse.h"
#include <string.h>
#include <strings.h>

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

static void print_status(bvstk_cli_t *cli, uint8_t service)
{
    dcp2_stream_sim_status_t status;

    if (!bvstk_stream_sim_get_status(cli->streams, service, &status)) {
        bvstk_cli_text(cli, "ERR (unknown stream service)\r\n");
        return;
    }
    cli->result = 0;
    bvstk_cli_writef(cli,
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

void bvstk_stream_cli_help(bvstk_cli_t *cli, uint8_t service)
{
    const char *name = dcp2_stream_sim_service_name(service);

    bvstk_cli_writef(cli, "%s stream usage:\r\n", name);
    bvstk_cli_writef(cli,
                  "  %s stream fake enable [period_ms] "
                  "[counter|ramp|toggle] [words]\r\n",
                  name);
    bvstk_cli_writef(cli, "  %s stream fake disable\r\n", name);
    bvstk_cli_writef(cli, "  %s stream fake status\r\n", name);
    bvstk_cli_writef(cli, "  %s stream status\r\n", name);
    bvstk_cli_text(cli,
              "  defaults: period_ms=100, pattern=counter, words=4; "
              "period range=10..60000 ms, words range=1..64\r\n");
}

bool bvstk_stream_cli(bvstk_cli_t *cli, uint8_t service, int argc, char **argv)
{
    int index = 0;
    char *mode = index < argc ? argv[index++] : NULL;
    char *action;
    cli->result = 1;

    if (mode == NULL || strcasecmp(mode, "-h") == 0 ||
        strcasecmp(mode, "--help") == 0) {
        cli->result = 0;
        bvstk_stream_cli_help(cli, service);
        return true;
    }

    if (strcasecmp(mode, "status") == 0) {
        if (index < argc) { cli->result = 2; bvstk_cli_text(cli, "ERR (too many arguments)\r\n"); return true; }
        if (service == DCP2_STREAM_SERVICE_I2C || service == DCP2_STREAM_SERVICE_MDIO)
            bvstk_cli_text(cli, "Real observations: active RAM journal; subscribe using DCP2 STREAM.\r\n");
        print_status(cli, service);
        return true;
    }

    if (strcasecmp(mode, "fake") != 0) {
        if (strcasecmp(mode, "enable") == 0 ||
            strcasecmp(mode, "disable") == 0) {
            bvstk_cli_text(cli, service == DCP2_STREAM_SERVICE_I2C || service == DCP2_STREAM_SERVICE_MDIO ?
                "ERR (real observations are always collected; use DCP2 subscribe/unsubscribe)\r\n" :
                "ERR (real stream source unavailable; use fake stream)\r\n");
        } else {
            bvstk_cli_text(cli, "ERR (expected fake or status)\r\n");
        }
        return true;
    }

    action = (index < argc ? argv[index++] : NULL);
    if (action == NULL || strcasecmp(action, "-h") == 0 ||
        strcasecmp(action, "--help") == 0) {
        cli->result = 0;
        bvstk_stream_cli_help(cli, service);
        return true;
    }
    if (strcasecmp(action, "status") == 0) {
        if (index < argc) { cli->result = 2; bvstk_cli_text(cli, "ERR (too many arguments)\r\n"); return true; }
        print_status(cli, service);
        return true;
    }
    if (strcasecmp(action, "disable") == 0 ||
        strcasecmp(action, "off") == 0) {
        if (index < argc) { cli->result = 2; bvstk_cli_text(cli, "ERR (too many arguments)\r\n"); return true; }
        if (!bvstk_stream_sim_configure(cli->streams, (uint32_t)bvstk_platform_now_ms(), service,
                                       false,
                                       DCP2_STREAM_SIM_DEFAULT_PERIOD_MS,
                                       DCP2_STREAM_SIM_PATTERN_COUNTER,
                                       DCP2_STREAM_SIM_DEFAULT_WORDS)) {
            bvstk_cli_text(cli, "ERR (failed to disable fake stream)\r\n");
            return true;
        }
        cli->result = 0;
        bvstk_cli_writef(cli,
                      "OK %s stream fake disabled\r\n",
                      dcp2_stream_sim_service_name(service));
        return true;
    }
    if (strcasecmp(action, "enable") != 0 &&
        strcasecmp(action, "on") != 0) {
        bvstk_cli_text(cli, "ERR (expected enable, disable or status)\r\n");
        return true;
    }

    {
        char *period_text = (index < argc ? argv[index++] : NULL);
        char *pattern_text = (index < argc ? argv[index++] : NULL);
        char *words_text = (index < argc ? argv[index++] : NULL);
        uint32_t period_ms = DCP2_STREAM_SIM_DEFAULT_PERIOD_MS;
        dcp2_stream_sim_pattern_t pattern = DCP2_STREAM_SIM_PATTERN_COUNTER;
        uint16_t words = DCP2_STREAM_SIM_DEFAULT_WORDS;

        if (period_text != NULL && !parse_period(period_text, &period_ms)) {
            bvstk_cli_text(cli, "ERR (period_ms must be 10..60000)\r\n");
            return true;
        }
        if (pattern_text != NULL && !parse_pattern(pattern_text, &pattern)) {
            bvstk_cli_text(cli, "ERR (pattern must be counter, ramp or toggle)\r\n");
            return true;
        }
        if (words_text != NULL && !parse_words(words_text, &words)) {
            bvstk_cli_text(cli, "ERR (words must be 1..64)\r\n");
            return true;
        }
        if (index < argc) { bvstk_cli_text(cli, "ERR (too many arguments)\r\n"); return true; }
        if (!bvstk_stream_sim_configure(cli->streams, (uint32_t)bvstk_platform_now_ms(), service,
                                       true,
                                       period_ms,
                                       pattern,
                                       words)) {
            bvstk_cli_text(cli, "ERR (failed to enable fake stream)\r\n");
            return true;
        }
        cli->result = 0;
        bvstk_cli_writef(cli,
                      "OK %s stream fake enabled period_ms=%lu pattern=%s "
                      "words=%u\r\n",
                      dcp2_stream_sim_service_name(service),
                      (unsigned long)period_ms,
                      dcp2_stream_sim_pattern_name(pattern),
                      (unsigned)words);
    }
    return true;
}
