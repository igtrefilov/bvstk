#include <stdint.h>

#include "lwip/sockets.h"
#include "xil_io.h"

/* Binary register access helpers; currently unused but kept for parity with the previous monolith. */

static void __attribute__((unused)) reg_write(uint8_t *data_buffer, int data_length, uint8_t auto_increment, int socket_fd)
{
    uint16_t total_bytes = (uint16_t)((data_buffer[1] << 8) | data_buffer[2]);
    int total_words = (total_bytes > 5) ? (int)((total_bytes - 5) / 4) : 0;
    uint32_t address = ((uint32_t)data_buffer[4] << 24) |
                       ((uint32_t)data_buffer[5] << 16) |
                       ((uint32_t)data_buffer[6] << 8)  |
                       ((uint32_t)data_buffer[7] << 0);
    for (int i = 0; i < total_words; i++) {
        if (8 + i * 4 + 3 >= data_length) return;
        uint32_t word_to_write = ((uint32_t)data_buffer[8 + i * 4] << 24) |
                                 ((uint32_t)data_buffer[8 + i * 4 + 1] << 16) |
                                 ((uint32_t)data_buffer[8 + i * 4 + 2] << 8)  |
                                 ((uint32_t)data_buffer[8 + i * 4 + 3] << 0);
        Xil_Out32(address, word_to_write);
        if (auto_increment) address += 4;
    }
    (void)socket_fd;
}

static void __attribute__((unused)) reg_read(uint8_t *data_buffer, int data_length, uint8_t auto_increment, int socket_fd)
{
    uint16_t total_bytes = (uint16_t)((data_buffer[1] << 8) | data_buffer[2]);
    uint16_t words_to_read = (uint16_t)((data_buffer[8] << 8) | data_buffer[9]);
    uint32_t address = ((uint32_t)data_buffer[4] << 24) |
                       ((uint32_t)data_buffer[5] << 16) |
                       ((uint32_t)data_buffer[6] << 8)  |
                       ((uint32_t)data_buffer[7] << 0);
    for (uint16_t i = 0; i < words_to_read; i++) {
        uint32_t word_read = Xil_In32(address);
        data_buffer[10 + i * 4]     = (uint8_t)((word_read >> 24) & 0xFF);
        data_buffer[10 + i * 4 + 1] = (uint8_t)((word_read >> 16) & 0xFF);
        data_buffer[10 + i * 4 + 2] = (uint8_t)((word_read >> 8)  & 0xFF);
        data_buffer[10 + i * 4 + 3] = (uint8_t)(word_read & 0xFF);
        if (auto_increment) address += 4;
    }
    total_bytes = (uint16_t)(words_to_read * 4 + 2 + 4 + 1);
    data_buffer[2] = (uint8_t)(total_bytes & 0xFF);
    data_buffer[1] = (uint8_t)(total_bytes >> 8);
    data_length = (int)total_bytes + 3;
    (void)lwip_write(socket_fd, data_buffer, data_length);
}

static void __attribute__((unused)) select_reg_write_read(uint8_t *data_buffer, int data_length, int socket_fd)
{
    if (data_length < 8) return;
    uint8_t wr_rd_flag = (uint8_t)(data_buffer[3] & 0x80);
    uint8_t auto_increment = (uint8_t)(data_buffer[3] & 0x40);
    if (wr_rd_flag) reg_write(data_buffer, data_length, auto_increment, socket_fd);
    else reg_read(data_buffer, data_length, auto_increment, socket_fd);
}
