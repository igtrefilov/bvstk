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
#include "dma_proc/dma_processing.h"
#include "mqtt_proc/mqtt_processing.h"
#include "sntp_proc/sntp_processing.h"

#define MAX_CONNECTIONS 8

extern QueueHandle_t dmaQueue;

int p_count = 0;

int eth_socket;
int client_socket = -1;
int client_active = 0;

u16_t echo_port = 8888;
u64_t actual_ntp_time_us;

//void handle_client_request(void *socket_ptr);
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

void handle_client_request(void *parameters) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while (1) {
        bytes_received = read(client_socket, buffer, BUFFER_SIZE - 1);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            xil_printf("Received %d bytes\r\n", bytes_received);
            process_received_data((uint8_t *)buffer, bytes_received, client_socket);
        } else {
            xil_printf("Client disconnected\r\n");
            break;
        }
    }
    close(client_socket);
    client_socket = -1;
    client_active = 0;
    vTaskDelete(NULL);
}

void process_received_data(uint8_t *data_buffer, int data_length, int socket_fd) {
    if (data_length < 1) {
        xil_printf("ERROR: Buffer size too small.\r\n");
        return;
    }
    switch (data_buffer[0]) {
        case 0x00: // System ping
            xil_printf("%02X - System ping received, resetting timer.\r\n", data_buffer[0]);
            pong(data_buffer, data_length, socket_fd);
            break;
        case 0x01: // Register read/write command
            xil_printf("%02X - Register read/write command\r\n", data_buffer[0]);
            select_reg_write_read(data_buffer, data_length, socket_fd);
            break;
        case 0x02: // Configure DMA
			xil_printf("%02X - Configure DMA\r\n", data_buffer[0]);
			break;
        case 0xff: // Error packet
            xil_printf("%02X - Error packet received.\r\n", data_buffer[0]);
            break;
        default:
            xil_printf("%02X - Unknown command.\r\n", data_buffer[0]);
            break;
    }
}

void pong(uint8_t *data_buffer, int data_length, int socket_fd){
	int send_result = write(socket_fd, data_buffer, data_length);
	if(send_result < 0) {
		xil_printf("ERROR: Failed to send data, error code: %d\r\n", send_result);
		return;
	} else if (send_result < data_length) {
		xil_printf("WARNING: Only %d of %d bytes sent.\r\n", send_result, data_length);
	} else {
		xil_printf("Data sent successfully (%d bytes).\r\n", send_result);
	}
}

void select_reg_write_read(uint8_t *data_buffer, int data_length, int socket_fd) {
    if (data_length < 8) {
        xil_printf("ERROR: Buffer size too small for register operation.\r\n");
        return;
    }

    uint8_t wr_rd_flag = data_buffer[3] & 0x80;
    uint8_t auto_increment = data_buffer[3] & 0x40;

    if (wr_rd_flag) {
        xil_printf("Executing write operation.\r\n");
        reg_write(data_buffer, data_length, auto_increment, socket_fd);
    } else {
        xil_printf("Executing read operation.\r\n");
        reg_read(data_buffer, data_length, auto_increment, socket_fd);
    }
}

void reg_write(uint8_t *data_buffer, int data_length, uint8_t auto_increment, int socket_fd) {
    uint16_t total_bytes = (data_buffer[1] << 8) | data_buffer[2];
    int total_words = (total_bytes > 5) ? (total_bytes - 5) / 4 : 0;
    uint32_t address = (data_buffer[4] << 24) | (data_buffer[5] << 16) | (data_buffer[6] << 8) | data_buffer[7];

    for (int i = 0; i < total_words; i++) {
        if (8 + i * 4 + 3 >= data_length) {
            xil_printf("ERROR: Buffer overflow during write.\r\n");
            return;
        }

        uint32_t word_to_write = (data_buffer[8 + i * 4] << 24) |
                                  (data_buffer[8 + i * 4 + 1] << 16) |
                                  (data_buffer[8 + i * 4 + 2] << 8) |
                                  data_buffer[8 + i * 4 + 3];

        xil_printf("Writing word - %08X to address - %08X\r\n", word_to_write, address);
        Xil_Out32(address, word_to_write);

        if (auto_increment) {
            address += 4; // Increment address by word size if auto_increment is enabled
        }
    }
}

void reg_read(uint8_t *data_buffer, int data_length, uint8_t auto_increment, int socket_fd) {
    uint16_t total_bytes = (data_buffer[1] << 8) | data_buffer[2];
    uint16_t words_to_read = (data_buffer[8] << 8) | data_buffer[9];
    uint32_t address = (data_buffer[4] << 24) | (data_buffer[5] << 16) | (data_buffer[6] << 8) | data_buffer[7];

    xil_printf("Input buffer: ");
        for(int i=0; i< data_length; i++){
        	xil_printf("%02X", data_buffer[i]);
        }
        xil_printf("\r\n");

    for (int i = 0; i < words_to_read; i++) {
        uint32_t word_read = Xil_In32(address);

        // Store read data into buffer
        data_buffer[10 + i * 4] = (word_read >> 24) & 0xFF;
        data_buffer[10 + i * 4 + 1] = (word_read >> 16) & 0xFF;
        data_buffer[10 + i * 4 + 2] = (word_read >> 8) & 0xFF;
        data_buffer[10 + i * 4 + 3] = word_read & 0xFF;

        xil_printf("Read word - %08X from address - %08X\r\n", word_read, address);

        if (auto_increment) {
            address += 4; // Increment address for next word
        }
    }

    total_bytes = (words_to_read * 4) + 2 + 4 + 1;
    data_buffer[2] = total_bytes & 0xFF;
    data_buffer[1] = total_bytes >> 8;
    data_length = total_bytes + 3;

    xil_printf("Output buffer: ");
    for(int i=0; i< data_length; i++){
    	xil_printf("%02X", data_buffer[i]);
    }
    xil_printf("\r\n");

    int send_result = write(socket_fd, data_buffer, data_length);
    if (send_result < 0) {
        xil_printf("ERROR: Failed to send data, error code: %d\n", send_result);
        return;
    } else if (send_result < data_length) {
        xil_printf("WARNING: Only %d of %d bytes sent.\r\n", send_result, data_length);
    } else {
        xil_printf("Data sent successfully (%d bytes).\r\n", send_result);
    }
}

uint16_t swap_endianness_16(uint16_t value) {
    return (value >> 8) | (value << 8);
}

uint32_t swap_endianness_32(uint32_t value) {
    return ((value >> 24) & 0x000000FF) |
           ((value >> 8)  & 0x0000FF00) |
           ((value << 8)  & 0x00FF0000) |
           ((value << 24) & 0xFF000000);
}

uint64_t swap_endianness_64(uint64_t value) {
    return ((value >> 56) & 0x00000000000000FFULL) |
           ((value >> 40) & 0x000000000000FF00ULL) |
           ((value >> 24) & 0x0000000000FF0000ULL) |
           ((value >> 8)  & 0x00000000FF000000ULL) |
           ((value << 8)  & 0x000000FF00000000ULL) |
           ((value << 24) & 0x0000FF0000000000ULL) |
           ((value << 40) & 0x00FF000000000000ULL) |
           ((value << 56) & 0xFF00000000000000ULL);
}

void ddr_eth_task(void *parameters) {
    uint8_t requestType = 0x03;
    uint16_t dataSize;
    int inputWords;
    uint8_t *output_buffer;

    struct {
        u32 *buffer;
        uint16_t length;
    } dmaData;

    (void)parameters;

    while (1) {
    	if (xQueueReceive(dmaQueue, &dmaData, portMAX_DELAY) == pdPASS) {
            dataSize = dmaData.length;
            inputWords = dmaData.length / 4;
            size_t tcpDataSize = sizeof(requestType) + sizeof(dataSize) + sizeof(actual_ntp_time_us) + dataSize;

            p_count++;
            xil_printf("%d. ", p_count);
            for(int i=0; i < inputWords;i++){
            	xil_printf("0x%08X ", dmaData.buffer[i]);
            }
            xil_printf("\r\n");

            get_ntp_time_us_ptr(&actual_ntp_time_us);
            //print_ntp_time(&actual_ntp_time_us);

            output_buffer = malloc(tcpDataSize);
            if (output_buffer == NULL) {
                xil_printf("Memory allocation failed\r\n");
                continue;
            }

            size_t offset = 0;

            memcpy(output_buffer + offset, &requestType, sizeof(requestType));
            offset += sizeof(requestType);

            uint16_t swappedDataSize = swap_endianness_16(sizeof(actual_ntp_time_us) + dataSize);
            memcpy(output_buffer + offset, &swappedDataSize, sizeof(swappedDataSize));
            offset += sizeof(swappedDataSize);

            uint64_t swappedTime = swap_endianness_64(actual_ntp_time_us);
            memcpy(output_buffer + offset, &swappedTime, sizeof(swappedTime));
            offset += sizeof(swappedTime);

            for (int i = 0; i < inputWords; i++) {
                uint32_t swappedValue = swap_endianness_32(dmaData.buffer[i]);
                memcpy(output_buffer + offset, &swappedValue, sizeof(swappedValue));
                offset += sizeof(swappedValue);
            }

            /*xil_printf("output_buffer: 0x");
            for(int i=0; i<tcpDataSize; i++){
            	xil_printf("%02X", output_buffer[i]);
            }
            xil_printf("\r\n");*/

            /*send data via mqtt*/
            mqtt_publish_binary(output_buffer, tcpDataSize);
            if (!client_active || client_socket < 0) {
                free(output_buffer);
                continue;
            }
            /*send data via tcp*/
			ssize_t bytesSent = write(client_socket, output_buffer, tcpDataSize);
			if (bytesSent < 0) {
				xil_printf("Failed to send data to client socket\r\n");
				close(client_socket);
				client_socket = -1;
				client_active = 0;
			} else {
				//xil_printf("Data sent to client %d: %d bytes\r\n", i, (int)bytesSent);
			}
			free(output_buffer);
        }
    }
}

void init_ddr_eth_system(void) {
    dmaQueue = xQueueCreate(MAX_BUFFERS_COUNT, sizeof(struct { u32 *buffer; u32 length; }));

    if (dmaQueue == NULL) {
        xil_printf("Failed to create DMA queue\r\n");
        return;
    }
    xTaskCreate(ddr_eth_task, "ddr_eth_task", 1024, NULL, tskIDLE_PRIORITY, NULL);
}

void echo_application_thread(void *parameters) {
    struct sockaddr_in address, remote;
    int size = sizeof(remote);

    if ((eth_socket = lwip_socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        xil_printf("Socket creation failed\r\n");
        vTaskDelete(NULL);
        return;
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(echo_port);
    address.sin_addr.s_addr = INADDR_ANY;

    if (lwip_bind(eth_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        xil_printf("Bind failed\r\n");
        lwip_close(eth_socket);
        vTaskDelete(NULL);
        return;
    }

    lwip_listen(eth_socket, 1);
    xil_printf("Server listening on port %d\r\n", echo_port);

    while (1) {
        if (client_active) {
            vTaskDelay(100);
            continue;
        }

        client_socket = lwip_accept(eth_socket, (struct sockaddr *)&remote, (socklen_t *)&size);
        if (client_socket < 0) {
            xil_printf("Accept failed\r\n");
            continue;
        }

        if (!client_active) {
            client_active = 1;
            xil_printf("Client connected\r\n");

            sys_thread_new("handle_client_request", handle_client_request, 0,
            		THREAD_STACKSIZE,
					tskIDLE_PRIORITY);
            xil_printf("Waiting for next client...\r\n");
        } else {
            close(client_socket);
            xil_printf("New client rejected: server is busy\r\n");
        }
    }
}

/*temp function*/

void print_ntp_time(const u64_t *ntp_microseconds_ptr) {
    if (!ntp_microseconds_ptr) return;

    uint64_t ntp_microseconds = *ntp_microseconds_ptr;
    time_t ntp_seconds = ntp_microseconds / 1000000ULL;

    time_t unix_time = ntp_seconds - 2208988800ULL;

    struct tm *tm_info = localtime(&unix_time);

    if (tm_info) {
        xil_printf("Current date and time: %04d-%02d-%02d %02d:%02d:%02d \r\n",
               tm_info->tm_year + 1900,
               tm_info->tm_mon + 1,
               tm_info->tm_mday,
               tm_info->tm_hour,
               tm_info->tm_min,
               tm_info->tm_sec);
    } else {
        xil_printf("Invalid time.\r\n");
    }
}
