#include "bvstk_status.h"

const char *bvstk_status_string(bvstk_status_t status)
{
    switch (status) {
    case BVSTK_OK:              return "ok";
    case BVSTK_ERR_MALFORMED:  return "malformed";
    case BVSTK_ERR_UNSUPPORTED:return "unsupported";
    case BVSTK_ERR_DENIED:     return "denied";
    case BVSTK_ERR_BUSY:       return "busy";
    case BVSTK_ERR_TIMEOUT:    return "timeout";
    case BVSTK_ERR_RANGE:      return "range";
    case BVSTK_ERR_NOT_READY:  return "not-ready";
    case BVSTK_ERR_NOT_FOUND:  return "not-found";
    case BVSTK_ERR_IO:         return "io";
    case BVSTK_ERR_INTERNAL:   return "internal";
    default:                   return "unknown";
    }
}
