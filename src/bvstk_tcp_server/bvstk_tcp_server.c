#include "bvstk_tcp_server.h"

int p_count = 0;

int eth_socket;
int client_socket = -1;
int client_active = 0;

u16_t echo_port = 8888;
u64_t actual_ntp_time_us;

void start_tcp_server(void){
	sys_thread_new("tcp_server_thrd", tcp_server_thread, 0,
			THREAD_STACKSIZE,
			tskIDLE_PRIORITY);
}

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

void tcp_server_thread(void *p) {
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

