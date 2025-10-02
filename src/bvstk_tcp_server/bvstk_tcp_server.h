#ifndef BVSTK_TCP_SERVER_H
#define BVSTK_TCP_SERVER_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>

#include "lwip/sockets.h"
#include "netif/xadapter.h"
#include "lwipopts.h"
#include "xil_printf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "xgpio.h"
#include "xil_cache.h"

#include "../bvstk_lan/bvstk_lan.h"

#define MAX_CONNECTIONS 8
#define BUFFER_SIZE 1024

void start_tcp_server(void);
void handle_client_request(void *parameters);
void process_received_data(uint8_t *data_buffer, int data_length, int socket_fd);
void pong(uint8_t *data_buffer, int data_length, int socket_fd);
void select_reg_write_read(uint8_t *data_buffer, int data_length, int socket_fd);
void reg_write(uint8_t *data_buffer, int data_length, uint8_t auto_increment, int socket_fd);
void reg_read(uint8_t *data_buffer, int data_length, uint8_t auto_increment, int socket_fd);
uint16_t swap_endianness_16(uint16_t value);
uint32_t swap_endianness_32(uint32_t value);
uint64_t swap_endianness_64(uint64_t value);
void ddr_eth_task(void *parameters);
void init_ddr_eth_system(void);
void print_ntp_time(const u64_t *ntp_microseconds_ptr);
void get_ntp_time_us_ptr(u64_t *ntp_time_us_ptr);
void tcp_server_thread(void *p);

#endif // BVSTK_TCP_SERVER_H
