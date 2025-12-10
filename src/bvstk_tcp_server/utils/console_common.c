#include "console_common.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "lwip/sockets.h"
#include "../../sd_card/sd_card.h"

#define PROMPT_MAX 80

void console_session_init(console_session_t *s)
{
    if (!s) return;
    strncpy(s->cwd, SD_ROOT, CONSOLE_CWD_LEN - 1);
    s->cwd[CONSOLE_CWD_LEN - 1] = '\0';
}

void write_str(int fd, const char *s)
{
    (void)lwip_write(fd, s, strlen(s));
}

unsigned long parse_num(const char *s, bool *ok)
{
    char *end = NULL;
    unsigned long base = 10;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) base = 16;
    unsigned long v = strtoul(s, &end, base);
    *ok = (end && *end == '\0');
    return v;
}

uint16_t swap_endianness_16(uint16_t value)
{
    return (uint16_t)((value >> 8) | (value << 8));
}

uint32_t swap_endianness_32(uint32_t value)
{
    return ((value >> 24) & 0x000000FFu) |
           ((value >> 8)  & 0x0000FF00u) |
           ((value << 8)  & 0x00FF0000u) |
           ((value << 24) & 0xFF000000u);
}

uint64_t swap_endianness_64(uint64_t value)
{
    return ((value >> 56) & 0x00000000000000FFULL) |
           ((value >> 40) & 0x000000000000FF00ULL) |
           ((value >> 24) & 0x0000000000FF0000ULL) |
           ((value >> 8)  & 0x00000000FF000000ULL) |
           ((value << 8)  & 0x000000FF00000000ULL) |
           ((value << 24) & 0x0000FF0000000000ULL) |
           ((value << 40) & 0x00FF000000000000ULL) |
           ((value << 56) & 0xFF00000000000000ULL);
}

void console_print_prompt(int fd, const console_session_t *session)
{
    char prompt[PROMPT_MAX];
    const char *cwd = (session && session->cwd[0]) ? session->cwd : SD_ROOT;
    const size_t root_len = strlen(SD_ROOT);
    const char *rel = cwd;
    if (strncmp(cwd, SD_ROOT, root_len) == 0) {
        rel = cwd + root_len;
        if (*rel == '/') rel++;
    }
    if (rel && *rel) {
        snprintf(prompt, sizeof(prompt), "Zynq/%s> ", rel);
    } else {
        snprintf(prompt, sizeof(prompt), "Zynq> ");
    }
    write_str(fd, prompt);
}
