#include "shared/cli/bvstk_i2c_completion.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

typedef struct {
    const char *text;
    size_t length;
} token_view_t;

static size_t split_tokens_upto(const char *line,
                                size_t upto,
                                token_view_t *tokens,
                                size_t token_capacity)
{
    size_t count = 0U;
    size_t position = 0U;

    if (line == NULL || tokens == NULL || token_capacity == 0U) {
        return 0U;
    }
    while (position < upto) {
        size_t start;

        while (position < upto &&
               (line[position] == ' ' || line[position] == '\t')) {
            ++position;
        }
        if (position >= upto) {
            break;
        }
        start = position;
        while (position < upto &&
               line[position] != ' ' && line[position] != '\t') {
            ++position;
        }
        if (count >= token_capacity) {
            return count;
        }
        tokens[count].text = line + start;
        tokens[count].length = position - start;
        ++count;
    }
    return count;
}

static int token_equals(const char *line,
                        token_view_t token,
                        const char *word)
{
    size_t word_length;

    if (line == NULL || word == NULL) {
        return 0;
    }
    word_length = strlen(word);
    return token.length == word_length &&
           strncasecmp(line + (token.text - line), word, word_length) == 0;
}

static int add_match(const char *prefix,
                     const char *candidate,
                     bvstk_i2c_completion_result_t *result)
{
    size_t prefix_length;

    if (prefix == NULL || candidate == NULL || result == NULL) {
        return 0;
    }
    prefix_length = strlen(prefix);
    if (prefix_length != 0U &&
        strncasecmp(candidate, prefix, prefix_length) != 0) {
        return 0;
    }
    if (result->match_count >= BVSTK_I2C_COMPLETION_MAX_MATCHES) {
        return 1;
    }
    strncpy(result->candidates[result->match_count],
            candidate,
            BVSTK_I2C_COMPLETION_WORD_MAX - 1U);
    result->candidates[result->match_count]
             [BVSTK_I2C_COMPLETION_WORD_MAX - 1U] = '\0';
    ++result->match_count;
    return 1;
}

static void complete_words(const char *prefix,
                           const char *const *words,
                           size_t word_count,
                           bvstk_i2c_completion_result_t *result)
{
    size_t index;

    if (prefix == NULL || words == NULL || result == NULL) {
        return;
    }
    for (index = 0U; index < word_count; ++index) {
        (void)add_match(prefix, words[index], result);
    }
}

static void complete_selector(
    const char *prefix,
    const bvstk_i2c_completion_device_t *devices,
    size_t device_count,
    bvstk_i2c_completion_result_t *result)
{
    static const char *const head_words[] = {"list", "-h", "--help"};
    size_t index;

    complete_words(prefix,
                   head_words,
                   sizeof(head_words) / sizeof(head_words[0]),
                   result);
    if (devices == NULL) {
        return;
    }
    for (index = 0U; index < device_count; ++index) {
        if (prefix[0] == '@') {
            char address[sizeof(result->candidates[0])];

            (void)snprintf(address,
                           sizeof(address),
                           "@0x%02X",
                           (unsigned)(devices[index].addr_7b & 0x7FU));
            (void)add_match(prefix, address, result);
        } else if (devices[index].name[0] != '\0') {
            (void)add_match(prefix, devices[index].name, result);
        }
    }
}

static void complete_operation(const char *prefix,
                               bvstk_i2c_completion_result_t *result)
{
    static const char *const operations[] = {
        "info", "r", "w", "addr", "address", "policy"
    };

    complete_words(prefix,
                   operations,
                   sizeof(operations) / sizeof(operations[0]),
                   result);
}

static void complete_policy(const char *prefix,
                            const token_view_t *tokens,
                            size_t token_count,
                            const char *line,
                            bvstk_i2c_completion_result_t *result)
{
    static const char *const policy_words[] = {
        "show", "set", "whitelist", "blacklist"
    };
    static const char *const show_words[] = {
        "rules", "whitelist", "blacklist"
    };
    static const char *const set_words[] = {"whitelist", "blacklist"};
    static const char *const edit_words[] = {"add", "del", "delete", "clear"};

    if (token_count < 2U || token_equals(line, tokens[1], "list")) {
        return;
    }
    if (token_count == 3U && token_equals(line, tokens[2], "policy")) {
        complete_words(prefix,
                       policy_words,
                       sizeof(policy_words) / sizeof(policy_words[0]),
                       result);
    } else if (token_count == 4U &&
               token_equals(line, tokens[2], "policy") &&
               token_equals(line, tokens[3], "show")) {
        complete_words(prefix,
                       show_words,
                       sizeof(show_words) / sizeof(show_words[0]),
                       result);
    } else if (token_count == 4U &&
               token_equals(line, tokens[2], "policy") &&
               token_equals(line, tokens[3], "set")) {
        complete_words(prefix,
                       set_words,
                       sizeof(set_words) / sizeof(set_words[0]),
                       result);
    } else if (token_count == 4U &&
               token_equals(line, tokens[2], "policy") &&
               (token_equals(line, tokens[3], "whitelist") ||
                token_equals(line, tokens[3], "blacklist"))) {
        complete_words(prefix,
                       edit_words,
                       sizeof(edit_words) / sizeof(edit_words[0]),
                       result);
    }
}

int bvstk_i2c_complete_line(
    const char *line,
    size_t line_length,
    size_t cursor,
    const bvstk_i2c_completion_device_t *devices,
    size_t device_count,
    bvstk_i2c_completion_result_t *result)
{
    token_view_t tokens[4];
    size_t token_count;
    size_t token_start;
    size_t prefix_length;
    char prefix[BVSTK_I2C_COMPLETION_WORD_MAX];

    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (line == NULL || result == NULL || line_length == 0U) {
        return 0;
    }
    if (cursor > line_length) {
        cursor = line_length;
    }
    token_start = cursor;
    while (token_start > 0U &&
           line[token_start - 1U] != ' ' &&
           line[token_start - 1U] != '\t') {
        --token_start;
    }
    prefix_length = cursor - token_start;
    if (prefix_length >= sizeof(prefix)) {
        return 0;
    }
    memcpy(prefix, line + token_start, prefix_length);
    prefix[prefix_length] = '\0';
    token_count = split_tokens_upto(line,
                                    token_start,
                                    tokens,
                                    sizeof(tokens) / sizeof(tokens[0]));

    /* Also make an exact `i2c<Tab>` useful, not just `i2c <Tab>`. */
    if (token_count == 0U && token_start == 0U &&
        prefix_length == strlen("i2c") &&
        strncasecmp(prefix, "i2c", prefix_length) == 0) {
        result->token_start = cursor;
        result->token_prefix_length = 0U;
        complete_selector("", devices, device_count, result);
        return result->match_count != 0U;
    }
    if (token_count == 0U || !token_equals(line, tokens[0], "i2c")) {
        return 0;
    }

    result->token_start = token_start;
    result->token_prefix_length = prefix_length;
    if (token_count == 1U) {
        complete_selector(prefix, devices, device_count, result);
    } else if (token_count == 2U &&
               !token_equals(line, tokens[1], "list")) {
        complete_operation(prefix, result);
    } else {
        complete_policy(prefix, tokens, token_count, line, result);
    }
    return result->match_count != 0U;
}

static size_t common_prefix_len(const char *first, const char *second)
{
    size_t index = 0U;

    if (first == NULL || second == NULL) {
        return 0U;
    }
    while (first[index] != '\0' && second[index] != '\0' &&
           tolower((unsigned char)first[index]) ==
               tolower((unsigned char)second[index])) {
        ++index;
    }
    return index;
}

size_t bvstk_i2c_completion_common_prefix_len(
    const char candidates[][BVSTK_I2C_COMPLETION_WORD_MAX],
    size_t candidate_count)
{
    size_t index;
    size_t length;

    if (candidates == NULL || candidate_count == 0U) {
        return 0U;
    }
    if (candidate_count > BVSTK_I2C_COMPLETION_MAX_MATCHES) {
        candidate_count = BVSTK_I2C_COMPLETION_MAX_MATCHES;
    }
    length = strlen(candidates[0]);
    for (index = 1U; index < candidate_count; ++index) {
        size_t current = common_prefix_len(candidates[0], candidates[index]);

        if (current < length) {
            length = current;
        }
        if (length == 0U) {
            break;
        }
    }
    return length;
}
