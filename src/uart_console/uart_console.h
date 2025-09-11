#ifndef UART_CONSOLE_H
#define UART_CONSOLE_H

#include "xuartps_hw.h"
#include "xuartps.h"
#include "xparameters.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

void TaskConsole(void *pvParameters);
void command(char *command_str);
void print_ip_address(struct netif *netif);
void print_netmask_address(struct netif *netif);
void print_gw_address(struct netif *netif);
void print_mac(unsigned char* mac_ethernet_address);
void print_iface(struct netif *netif);
void set_ip(char *ip_str);
void set_netmask(char *ip_str);
void set_gw(char *ip_str);

#endif // UART_CONSOLE_H
