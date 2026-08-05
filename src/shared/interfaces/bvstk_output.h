#ifndef BVSTK_SHARED_OUTPUT_H
#define BVSTK_SHARED_OUTPUT_H

#include <stddef.h>

typedef int (*bvstk_output_write_fn)(void *context,
                                     const void *data,
                                     size_t length);

typedef struct {
    bvstk_output_write_fn write_fn;
    void *context;
} bvstk_output_t;

static inline int bvstk_output_write(const bvstk_output_t *output,
                                     const void *data,
                                     size_t length)
{
    if (output == NULL || output->write_fn == NULL ||
        (data == NULL && length != 0U)) {
        return -1;
    }
    return output->write_fn(output->context, data, length);
}

#endif /* BVSTK_SHARED_OUTPUT_H */
