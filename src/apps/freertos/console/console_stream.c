#include "apps/freertos/console/console_stream.h"

#include <string.h>

#include "lwip/sockets.h"

typedef struct {
    int fd;
    bvstk_output_t output;
} console_stream_slot_t;

static console_stream_slot_t s_slots[CONSOLE_STREAM_FD_MAX - CONSOLE_STREAM_FD_MIN + 1];

static console_stream_slot_t *find_slot(int fd)
{
    if (fd < CONSOLE_STREAM_FD_MIN || fd > CONSOLE_STREAM_FD_MAX) return NULL;
    console_stream_slot_t *slot = &s_slots[fd - CONSOLE_STREAM_FD_MIN];
    return (slot->output.write_fn != NULL && slot->fd == fd) ? slot : NULL;
}

int console_stream_register(int fd, console_stream_write_fn write_fn, void *ctx)
{
    if (fd < CONSOLE_STREAM_FD_MIN || fd > CONSOLE_STREAM_FD_MAX || !write_fn) return -1;
    console_stream_slot_t *slot = &s_slots[fd - CONSOLE_STREAM_FD_MIN];
    if (slot->output.write_fn != NULL) return -1;
    slot->fd = fd;
    slot->output.write_fn = write_fn;
    slot->output.context = ctx;
    return 0;
}

void console_stream_unregister(int fd)
{
    console_stream_slot_t *slot = find_slot(fd);
    if (!slot) return;
    memset(slot, 0, sizeof(*slot));
}

int console_stream_write(int fd, const void *data, size_t len)
{
    if (!data && len != 0) return -1;
    console_stream_slot_t *slot = find_slot(fd);
    if (slot) return bvstk_output_write(&slot->output, data, len);
    if (fd >= 0) return lwip_write(fd, data, (int)len);
    return -1;
}
