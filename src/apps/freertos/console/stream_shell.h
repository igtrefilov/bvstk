#ifndef BVSTK_FREERTOS_STREAM_SHELL_H
#define BVSTK_FREERTOS_STREAM_SHELL_H

#include <stdbool.h>
#include <stdint.h>

bool dcp2_stream_shell_handle(uint8_t service, char **save, int fd);
void dcp2_stream_shell_help(uint8_t service, int fd);

#endif /* BVSTK_FREERTOS_STREAM_SHELL_H */
