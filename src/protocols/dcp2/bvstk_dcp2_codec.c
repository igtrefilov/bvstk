#include "protocols/dcp2/bvstk_dcp2_codec.h"

#include <string.h>

uint16_t bvstk_dcp2_read_be16(const uint8_t *data)
{
    return data == NULL ? 0U : (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

uint32_t bvstk_dcp2_read_be32(const uint8_t *data)
{
    if (data == NULL) {
        return 0U;
    }
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) |
           (uint32_t)data[3];
}

void bvstk_dcp2_write_be16(uint8_t *data, uint16_t value)
{
    if (data != NULL) {
        data[0] = (uint8_t)(value >> 8);
        data[1] = (uint8_t)value;
    }
}

void bvstk_dcp2_write_be32(uint8_t *data, uint32_t value)
{
    if (data != NULL) {
        data[0] = (uint8_t)(value >> 24);
        data[1] = (uint8_t)(value >> 16);
        data[2] = (uint8_t)(value >> 8);
        data[3] = (uint8_t)value;
    }
}

int bvstk_dcp2_decode_request(const uint8_t *frame,
                              size_t frame_size,
                              bvstk_dcp2_request_t *request)
{
    uint16_t payload_size;
    uint8_t operation;

    if (frame == NULL || request == NULL ||
        frame_size < BVSTK_DCP2_HEADER_SIZE + BVSTK_DCP2_MIN_PAYLOAD ||
        frame[0] != 'D' || frame[1] != 'C' || frame[2] != 'P' || frame[3] != '2') {
        return -1;
    }
    if (bvstk_dcp2_read_be16(frame + 4) != BVSTK_DCP2_VERSION) {
        return -1;
    }
    payload_size = bvstk_dcp2_read_be16(frame + 6);
    if (payload_size < BVSTK_DCP2_MIN_PAYLOAD ||
        payload_size > BVSTK_DCP2_MAX_PAYLOAD ||
        frame_size != BVSTK_DCP2_HEADER_SIZE + payload_size) {
        return -1;
    }
    operation = frame[9];
    if ((operation & (BVSTK_DCP2_OP_RESPONSE | BVSTK_DCP2_OP_EVENT)) != 0U ||
        bvstk_dcp2_read_be16(frame + 10) == 0U) {
        return -1;
    }
    request->service = frame[8];
    request->opcode = (uint8_t)(operation & BVSTK_DCP2_OP_MASK);
    request->sequence = bvstk_dcp2_read_be16(frame + 10);
    request->body = frame + 12;
    request->body_size = (uint16_t)(payload_size - BVSTK_DCP2_MIN_PAYLOAD);
    return 0;
}

int bvstk_dcp2_encode_response(uint8_t *frame,
                               size_t frame_capacity,
                               const bvstk_dcp2_request_t *request,
                               uint16_t status,
                               const uint8_t *body,
                               uint16_t body_size,
                               size_t *frame_size)
{
    uint16_t payload_size;
    size_t total_size;

    if (frame == NULL || request == NULL ||
        (body_size != 0U && body == NULL)) {
        return -1;
    }
    payload_size = (uint16_t)(BVSTK_DCP2_MIN_PAYLOAD + 2U + body_size);
    total_size = BVSTK_DCP2_HEADER_SIZE + payload_size;
    if (body_size > BVSTK_DCP2_MAX_PAYLOAD - 6U ||
        frame_capacity < total_size) {
        return -1;
    }
    frame[0] = 'D';
    frame[1] = 'C';
    frame[2] = 'P';
    frame[3] = '2';
    bvstk_dcp2_write_be16(frame + 4, BVSTK_DCP2_VERSION);
    bvstk_dcp2_write_be16(frame + 6, payload_size);
    frame[8] = request->service;
    frame[9] = (uint8_t)(BVSTK_DCP2_OP_RESPONSE |
                         (request->opcode & BVSTK_DCP2_OP_MASK));
    bvstk_dcp2_write_be16(frame + 10, request->sequence);
    bvstk_dcp2_write_be16(frame + 12, status);
    if (body_size != 0U) {
        memcpy(frame + 14, body, body_size);
    }
    if (frame_size != NULL) {
        *frame_size = total_size;
    }
    return 0;
}

uint16_t bvstk_dcp2_status_from_bvstk(bvstk_status_t status)
{
    switch (status) {
    case BVSTK_OK: return 0x0000U;
    case BVSTK_ERR_MALFORMED: return 0x0001U;
    case BVSTK_ERR_UNSUPPORTED: return 0x0002U;
    case BVSTK_ERR_DENIED: return 0x0003U;
    case BVSTK_ERR_BUSY: return 0x0004U;
    case BVSTK_ERR_TIMEOUT: return 0x0005U;
    case BVSTK_ERR_RANGE: return 0x0006U;
    default: return 0x0007U;
    }
}
