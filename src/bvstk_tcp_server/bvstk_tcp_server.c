#include "bvstk_tcp_server.h"
#include "../bvstk_smi/bvstk_smi.h"
#include <ctype.h>
#include <stdlib.h>
#include <strings.h>

#ifndef THREAD_STACKSIZE
#define THREAD_STACKSIZE 2048
#endif

void process_console_line(const char *line, int socket_fd);
static unsigned long parse_num(const char *s, bool *ok);

int p_count = 0;
int eth_socket;
int client_socket = -1;
int client_active = 0;
u16_t echo_port = 8888;
u64_t actual_ntp_time_us;
static volatile int close_requested = 0;

void start_tcp_server(void){
    sys_thread_new("tcp_server_thrd", tcp_server_thread, 0, THREAD_STACKSIZE, tskIDLE_PRIORITY);
}

void handle_client_request(void *parameters) {
    char buffer[BUFFER_SIZE];
    int bytes_received;
    char linebuf[256];
    size_t linelen = 0;
    const char *greeting = "BVSTK MDIO console ready.\r\nType 'help' for commands. Binary protocol still supported.\r\n> ";
    (void)write(client_socket, greeting, strlen(greeting));
    close_requested = 0;
    while (1) {
        bytes_received = read(client_socket, buffer, BUFFER_SIZE - 1);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            xil_printf("Received %d bytes\r\n", bytes_received);
            bool looks_text = false;
            for (int i = 0; i < bytes_received; ++i) {
                unsigned char c = (unsigned char)buffer[i];
                if (c == '\n' || c == '\r' || isprint(c)) { looks_text = true; }
                else { looks_text = false; break; }
            }
            if (looks_text) {
                for (int i = 0; i < bytes_received; ++i) {
                    char c = buffer[i];
                    if (c == '\r') continue;
                    if (c == '\n') {
                        linebuf[linelen] = '\0';
                        if (linelen > 0) {
                            process_console_line(linebuf, client_socket);
                            linelen = 0;
                        }
                        if (close_requested) break;
                        (void)write(client_socket, "> ", 2);
                    } else if (linelen + 1 < sizeof(linebuf)) {
                        linebuf[linelen++] = c;
                    }
                }
                if (close_requested) break;
            } else {
                process_received_data((uint8_t *)buffer, bytes_received, client_socket);
            }
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
        case 0x00:
            xil_printf("%02X - System ping received, resetting timer.\r\n", data_buffer[0]);
            pong(data_buffer, data_length, socket_fd);
            break;
        case 0x01:
            xil_printf("%02X - Register read/write command\r\n", data_buffer[0]);
            select_reg_write_read(data_buffer, data_length, socket_fd);
            break;
        case 0x02:
            xil_printf("%02X - Configure DMA\r\n", data_buffer[0]);
            break;
        case 0xff:
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
            address += 4;
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
        data_buffer[10 + i * 4] = (word_read >> 24) & 0xFF;
        data_buffer[10 + i * 4 + 1] = (word_read >> 16) & 0xFF;
        data_buffer[10 + i * 4 + 2] = (word_read >> 8) & 0xFF;
        data_buffer[10 + i * 4 + 3] = word_read & 0xFF;
        xil_printf("Read word - %08X from address - %08X\r\n", word_read, address);
        if (auto_increment) {
            address += 4;
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

static unsigned long parse_num(const char *s, bool *ok)
{
    char *end = NULL;
    unsigned long base = 10;
    if (s[0]=='0' && (s[1]=='x' || s[1]=='X')) base = 16;
    unsigned long v = strtoul(s, &end, base);
    *ok = (end && *end=='\0');
    return v;
}

void process_console_line(const char *line, int socket_fd)
{
    char tmp[256];
    strncpy(tmp, line, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    char *save = NULL;
    char *tok = strtok_r(tmp, " \t", &save);
    if (!tok) return;
    if (strcasecmp(tok, "mdio") == 0) {
        tok = strtok_r(NULL, " \t", &save);
        if (!tok) { (void)write(socket_fd, "ERR\r\n", 5); return; }
    }
    if (strcasecmp(tok, "help") == 0) {
        const char *help =
            "Commands:\r\n"
            "  r <phy> <reg>\r\n"
            "  w <phy> <reg> <data>\r\n"
            "  mdio r|w ...\r\n"
            "  quit\r\n";
        (void)write(socket_fd, help, strlen(help));
        return;
    }
    if (strcasecmp(tok, "quit") == 0 || strcasecmp(tok, "exit") == 0) {
        (void)write(socket_fd, "Bye!\r\n", 6);
        close_requested = 1;
        return;
    }
    if (strcasecmp(tok, "r") == 0) {
        char *s_phy = strtok_r(NULL, " \t", &save);
        char *s_reg = strtok_r(NULL, " \t", &save);
        if (!s_phy || !s_reg) { (void)write(socket_fd, "ERR\r\n", 5); return; }
        bool ok1=false, ok2=false;
        unsigned long phy = parse_num(s_phy, &ok1);
        unsigned long reg = parse_num(s_reg, &ok2);
        if (!ok1 || !ok2 || phy>31 || reg>31) { (void)write(socket_fd, "ERR\r\n", 5); return; }
        uint16_t val = 0;
        if (mdio_read_blocking((uint8_t)phy, (uint8_t)reg, &val, pdMS_TO_TICKS(2000))) {
            char out[64];
            int n = snprintf(out, sizeof(out), "OK 0x%04X %u\r\n", val, val);
            (void)write(socket_fd, out, n);
        } else {
            (void)write(socket_fd, "TIMEOUT\r\n", 9);
        }
        return;
    }
    if (strcasecmp(tok, "w") == 0) {
        char *s_phy = strtok_r(NULL, " \t", &save);
        char *s_reg = strtok_r(NULL, " \t", &save);
        char *s_dat = strtok_r(NULL, " \t", &save);
        if (!s_phy || !s_reg || !s_dat) { (void)write(socket_fd, "ERR\r\n", 5); return; }
        bool ok1=false, ok2=false, ok3=false;
        unsigned long phy = parse_num(s_phy, &ok1);
        unsigned long reg = parse_num(s_reg, &ok2);
        unsigned long dat = parse_num(s_dat, &ok3);
        if (!ok1 || !ok2 || !ok3 || phy>31 || reg>31 || dat>0xFFFF) { (void)write(socket_fd, "ERR\r\n", 5); return; }
        mdio_write((uint8_t)phy, (uint8_t)reg, (uint16_t)dat);
        (void)write(socket_fd, "OK\r\n", 4);
        return;
    }
    (void)write(socket_fd, "ERR\r\n", 5);
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
            sys_thread_new("handle_client_request", handle_client_request, 0, THREAD_STACKSIZE, tskIDLE_PRIORITY);
            xil_printf("Waiting for next client...\r\n");
        } else {
            close(client_socket);
            xil_printf("New client rejected: server is busy\r\n");
        }
    }
}
