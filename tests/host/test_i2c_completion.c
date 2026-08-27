#include <stdio.h>
#include <string.h>

#include "shared/cli/bvstk_i2c_completion.h"

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n",                \
                    __FILE__, __LINE__, #condition);                         \
            return 1;                                                        \
        }                                                                    \
    } while (0)

static int has_candidate(const bvstk_i2c_completion_result_t *result,
                         const char *candidate)
{
    size_t index;

    for (index = 0U; index < result->match_count; ++index) {
        if (strcmp(result->candidates[index], candidate) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    const bvstk_i2c_completion_device_t devices[] = {
        {.name = "axp15060", .addr_7b = 0x36U},
        {.name = "adc", .addr_7b = 0x48U}
    };
    bvstk_i2c_completion_result_t result;

    CHECK(bvstk_i2c_complete_line("i2c ", 4U, 4U,
                                  devices, 2U, &result));
    CHECK(result.token_start == 4U && result.token_prefix_length == 0U &&
          result.match_count == 5U && has_candidate(&result, "axp15060") &&
          has_candidate(&result, "adc") && has_candidate(&result, "list"));

    CHECK(bvstk_i2c_complete_line("i2c @", 5U, 5U,
                                  devices, 2U, &result));
    CHECK(result.match_count == 2U && has_candidate(&result, "@0x36") &&
          has_candidate(&result, "@0x48"));

    CHECK(bvstk_i2c_complete_line("i2c", 3U, 3U,
                                  devices, 2U, &result));
    CHECK(result.token_start == 3U && result.token_prefix_length == 0U &&
          result.match_count == 5U);

    CHECK(bvstk_i2c_complete_line("i2c ad", 6U, 6U,
                                  devices, 2U, &result));
    CHECK(result.match_count == 1U &&
          strcmp(result.candidates[0], "adc") == 0 &&
          result.token_start == 4U && result.token_prefix_length == 2U);

    CHECK(bvstk_i2c_complete_line("i2c axp15060 ", 13U, 13U,
                                  devices, 2U, &result));
    CHECK(result.match_count == 6U && has_candidate(&result, "policy") &&
          has_candidate(&result, "address"));

    CHECK(bvstk_i2c_complete_line("i2c axp15060 policy ", 20U, 20U,
                                  devices, 2U, &result));
    CHECK(result.match_count == 4U && has_candidate(&result, "show") &&
          has_candidate(&result, "blacklist"));

    CHECK(bvstk_i2c_complete_line("i2c axp15060 policy show ", 25U, 25U,
                                  devices, 2U, &result));
    CHECK(result.match_count == 3U && has_candidate(&result, "rules"));

    CHECK(bvstk_i2c_complete_line("i2c axp15060 policy whitelist ",
                                  30U, 30U, devices, 2U, &result));
    CHECK(result.match_count == 4U && has_candidate(&result, "clear"));

    CHECK(!bvstk_i2c_complete_line("i2c list ", 9U, 9U,
                                   devices, 2U, &result));
    CHECK(!bvstk_i2c_complete_line("ls ", 3U, 3U,
                                   devices, 2U, &result));
    CHECK(bvstk_i2c_completion_common_prefix_len(
              (const char[][BVSTK_I2C_COMPLETION_WORD_MAX]){
                  "whitelist", "whitecap"},
              2U) == 5U);
    return 0;
}
