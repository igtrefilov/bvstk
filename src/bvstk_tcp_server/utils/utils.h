#ifndef BVSTK_TCP_UTILS_H
#define BVSTK_TCP_UTILS_H

#include <stdint.h>
#include <stdbool.h>

#define CONSOLE_CWD_LEN 64
typedef struct {
    char cwd[CONSOLE_CWD_LEN];
} console_session_t;

void console_session_init(console_session_t *s);
void console_print_prompt(int fd, const console_session_t *s);
void write_str(int fd, const char *s);
unsigned long parse_num(const char *s, bool *ok);
uint16_t swap_endianness_16(uint16_t value);
uint32_t swap_endianness_32(uint32_t value);
uint64_t swap_endianness_64(uint64_t value);

void utils_reset_close(void);
int utils_should_close(void);

void process_console_line(const char *line, int socket_fd, console_session_t *session);
void process_received_data(uint8_t *data_buffer, int data_length, int socket_fd);

#endif
