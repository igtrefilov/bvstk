#ifndef BVSTK_SHARED_PARSE_H
#define BVSTK_SHARED_PARSE_H

#include <stdbool.h>
#include <stdint.h>

unsigned long parse_num(const char *text, bool *ok);
uint16_t swap_endianness_16(uint16_t value);
uint32_t swap_endianness_32(uint32_t value);
uint64_t swap_endianness_64(uint64_t value);

#endif /* BVSTK_SHARED_PARSE_H */
