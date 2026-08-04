#ifndef BVSTK_STATUS_H
#define BVSTK_STATUS_H

typedef enum {
    BVSTK_OK = 0,
    BVSTK_ERR_MALFORMED,
    BVSTK_ERR_UNSUPPORTED,
    BVSTK_ERR_DENIED,
    BVSTK_ERR_BUSY,
    BVSTK_ERR_TIMEOUT,
    BVSTK_ERR_RANGE,
    BVSTK_ERR_NOT_READY,
    BVSTK_ERR_NOT_FOUND,
    BVSTK_ERR_IO,
    BVSTK_ERR_INTERNAL
} bvstk_status_t;

const char *bvstk_status_string(bvstk_status_t status);

#endif
