#ifndef BVSTK_SHARED_I2C_COMPLETION_H
#define BVSTK_SHARED_I2C_COMPLETION_H

#include <stddef.h>
#include <stdint.h>

#include "shared/config/bvstk_config_model.h"

#define BVSTK_I2C_COMPLETION_MAX_MATCHES 16U
#define BVSTK_I2C_COMPLETION_WORD_MAX 64U

typedef struct {
    char name[I2C_CFG_NAME_MAX];
    uint8_t addr_7b;
} bvstk_i2c_completion_device_t;

typedef struct {
    size_t token_start;
    size_t token_prefix_length;
    size_t match_count;
    char candidates[BVSTK_I2C_COMPLETION_MAX_MATCHES]
                   [BVSTK_I2C_COMPLETION_WORD_MAX];
} bvstk_i2c_completion_result_t;

/*
 * Complete the I2C arguments at `cursor` in `line`.
 *
 * A return value of 1 means that the line is an I2C command and candidates
 * were produced.  `token_start` and `token_prefix_length` describe the part
 * of the current token that the caller may replace.  Only the first
 * BVSTK_I2C_COMPLETION_MAX_MATCHES candidates are stored.
 */
int bvstk_i2c_complete_line(
    const char *line,
    size_t line_length,
    size_t cursor,
    const bvstk_i2c_completion_device_t *devices,
    size_t device_count,
    bvstk_i2c_completion_result_t *result);

size_t bvstk_i2c_completion_common_prefix_len(
    const char candidates[][BVSTK_I2C_COMPLETION_WORD_MAX],
    size_t candidate_count);

#endif /* BVSTK_SHARED_I2C_COMPLETION_H */
