#include "shared/base/bvstk_parse.h"

#include <stdlib.h>

unsigned long parse_num(const char *text, bool *ok)
{
    char *end = NULL;
    int base = 10;
    unsigned long value;

    if (ok == NULL) {
        return 0UL;
    }
    *ok = false;
    if (text == NULL || *text == '\0') {
        return 0UL;
    }
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
    }
    value = strtoul(text, &end, base);
    *ok = end != text && end != NULL && *end == '\0';
    return value;
}

uint16_t swap_endianness_16(uint16_t value)
{
    return (uint16_t)((value >> 8) | (value << 8));
}

uint32_t swap_endianness_32(uint32_t value)
{
    return ((value >> 24) & UINT32_C(0x000000FF)) |
           ((value >> 8)  & UINT32_C(0x0000FF00)) |
           ((value << 8)  & UINT32_C(0x00FF0000)) |
           ((value << 24) & UINT32_C(0xFF000000));
}

uint64_t swap_endianness_64(uint64_t value)
{
    return ((value >> 56) & UINT64_C(0x00000000000000FF)) |
           ((value >> 40) & UINT64_C(0x000000000000FF00)) |
           ((value >> 24) & UINT64_C(0x0000000000FF0000)) |
           ((value >> 8)  & UINT64_C(0x00000000FF000000)) |
           ((value << 8)  & UINT64_C(0x000000FF00000000)) |
           ((value << 24) & UINT64_C(0x0000FF0000000000)) |
           ((value << 40) & UINT64_C(0x00FF000000000000)) |
           ((value << 56) & UINT64_C(0xFF00000000000000));
}
