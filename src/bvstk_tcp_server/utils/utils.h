#ifndef BVSTK_TCP_UTILS_H
#define BVSTK_TCP_UTILS_H

#include "console_common.h"

void utils_reset_close(void);
int utils_should_close(void);

void process_console_line(const char *line, int socket_fd, console_session_t *session);
void process_received_data(uint8_t *data_buffer, int data_length, int socket_fd);

#endif
