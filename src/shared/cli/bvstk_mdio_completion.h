#ifndef BVSTK_MDIO_COMPLETION_H
#define BVSTK_MDIO_COMPLETION_H
#include <stddef.h>
#include <stdint.h>
#define BVSTK_MDIO_COMPLETION_MAX_MATCHES 16U
#define BVSTK_MDIO_COMPLETION_WORD_MAX 64U
typedef struct { char name[32]; uint8_t phy_addr; } bvstk_mdio_completion_device_t;
typedef struct {
    size_t token_start, token_prefix_length, match_count;
    char candidates[BVSTK_MDIO_COMPLETION_MAX_MATCHES][BVSTK_MDIO_COMPLETION_WORD_MAX];
} bvstk_mdio_completion_result_t;
int bvstk_mdio_complete_line(const char *line, size_t length, size_t cursor,
    const bvstk_mdio_completion_device_t *devices, size_t count,
    bvstk_mdio_completion_result_t *result);
#endif
