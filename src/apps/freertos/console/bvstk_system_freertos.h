#ifndef BVSTK_SYSTEM_FREERTOS_H
#define BVSTK_SYSTEM_FREERTOS_H
#include "services/system/bvstk_system.h"
bvstk_system_t *bvstk_system_freertos(void);
int bvstk_system_freertos_command(int, char **, int fd);
bvstk_status_t bvstk_freertos_schedule_reboot(unsigned);
#include "shared/protocols/http/bvstk_http.h"
int bvstk_system_freertos_http(const http_request_t *,http_conn_t *);
#endif
