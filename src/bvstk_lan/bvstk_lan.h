#ifndef BVSTK_LAN_H
#define BVSTK_LAN_H

#include <stdio.h>
#include "xparameters.h"
#include "netif/xadapter.h"
#include "xil_printf.h"

#define THREAD_STACKSIZE 2048

void start_lan(void);
void lwip_init(void);
void lan_thread(void *p);
void tcp_server_thread(void *p);

#endif // BVSTK_LAN_H
