#ifndef BVSTK_DCP2_CODEC_H
#define BVSTK_DCP2_CODEC_H

#include <stddef.h>
#include <stdint.h>

#include "shared/base/bvstk_status.h"

#define BVSTK_DCP2_VERSION UINT16_C(2)
#define BVSTK_DCP2_HEADER_SIZE 8U
#define BVSTK_DCP2_MIN_PAYLOAD 4U
#define BVSTK_DCP2_MAX_PAYLOAD 4096U

enum {
    BVSTK_DCP2_SERVICE_PING = 0x00,
    BVSTK_DCP2_SERVICE_MEM = 0x01,
    BVSTK_DCP2_SERVICE_I2C = 0x02,
    BVSTK_DCP2_SERVICE_SMI = 0x03,
    BVSTK_DCP2_SERVICE_SPI = 0x04,
    BVSTK_DCP2_SERVICE_UART = 0x05,
    BVSTK_DCP2_SERVICE_NOTIFY = 0x06
};

enum {
    BVSTK_DCP2_OP_PING = 0x00,
    BVSTK_DCP2_OP_MEM_READ = 0x00,
    BVSTK_DCP2_OP_MEM_WRITE = 0x01,
    BVSTK_DCP2_OP_I2C_READ_REG = 0x00,
    BVSTK_DCP2_OP_I2C_WRITE_REG = 0x01,
    BVSTK_DCP2_OP_I2C_POLICY_SET = 0x02,
    BVSTK_DCP2_OP_SMI_READ = 0x00,
    BVSTK_DCP2_OP_SMI_WRITE = 0x01,
    BVSTK_DCP2_OP_SPI_TRANSFER = 0x00
};

enum {
    BVSTK_DCP2_OP_RESPONSE = 0x80,
    BVSTK_DCP2_OP_EVENT = 0x40,
    BVSTK_DCP2_OP_MASK = 0x3F
};

typedef struct {
    uint8_t service;
    uint8_t opcode;
    uint16_t sequence;
    const uint8_t *body;
    uint16_t body_size;
} bvstk_dcp2_request_t;

int bvstk_dcp2_decode_request(const uint8_t *frame,
                              size_t frame_size,
                              bvstk_dcp2_request_t *request);
int bvstk_dcp2_encode_response(uint8_t *frame,
                               size_t frame_capacity,
                               const bvstk_dcp2_request_t *request,
                               uint16_t status,
                               const uint8_t *body,
                               uint16_t body_size,
                               size_t *frame_size);
uint16_t bvstk_dcp2_status_from_bvstk(bvstk_status_t status);
uint16_t bvstk_dcp2_read_be16(const uint8_t *data);
uint32_t bvstk_dcp2_read_be32(const uint8_t *data);
void bvstk_dcp2_write_be16(uint8_t *data, uint16_t value);
void bvstk_dcp2_write_be32(uint8_t *data, uint32_t value);

#endif
