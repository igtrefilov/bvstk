#ifndef BVSTK_FREERTOS_CONSOLE_STREAM_H
#define BVSTK_FREERTOS_CONSOLE_STREAM_H

#include <stddef.h>

#include "shared/interfaces/bvstk_output.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bvstk_output_write_fn console_stream_write_fn;

/* Negative descriptors are reserved for non-TCP console transports. */
#define CONSOLE_STREAM_FD_MIN (-16)
#define CONSOLE_STREAM_FD_MAX (-1)

int console_stream_register(int fd, console_stream_write_fn write_fn, void *ctx);
void console_stream_unregister(int fd);
int console_stream_write(int fd, const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif
