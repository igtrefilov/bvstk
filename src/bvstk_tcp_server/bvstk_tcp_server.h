#ifndef BVSTK_TCP_SERVER_H
#define BVSTK_TCP_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#include "lwip/sockets.h"
#include "netif/xadapter.h"
#include "lwipopts.h"
#include "xil_printf.h"
#include "FreeRTOS.h"
#include "task.h"

#include "utils/utils.h"

#ifndef THREAD_STACKSIZE
#define THREAD_STACKSIZE 8192
#endif

#define MAX_CONNECTIONS 8
#define BUFFER_SIZE 1024

void start_tcp_server(void);
void tcp_server_thread(void *p);

#endif
