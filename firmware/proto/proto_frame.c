#include "proto_frame.h"

#include <string.h>

#define CRC32_POLY 0xEDB88320u

static void put_u32le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint32_t get_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint32_t proto_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;

    if (!data) return 0;

    for (i = 0; i < len; i++) {
        unsigned bit;
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (CRC32_POLY * (crc & 1u));
        }
    }
    return ~crc;
}

size_t proto_frame_wire_len(uint8_t payload_len) {
    if (payload_len > PROTO_MAX_PAYLOAD) return 0;
    return (size_t)PROTO_HEADER_LEN + (size_t)payload_len + (size_t)PROTO_AUTH_LEN;
}

proto_status_t proto_frame_encode(uint8_t *buf, size_t cap,
                                  const proto_frame_view_t *frame,
                                  size_t *out_len) {
    size_t wire_len;

    if (!buf || !frame || !out_len) return PROTO_ERR_ARGUMENT;
    if (frame->payload_len > PROTO_MAX_PAYLOAD) return PROTO_ERR_LENGTH;
    if (frame->payload_len > 0 && !frame->payload) return PROTO_ERR_ARGUMENT;
    if (frame->type < PROTO_TYPE_HELLO || frame->type > PROTO_TYPE_ERROR) {
        return PROTO_ERR_TYPE;
    }

    wire_len = proto_frame_wire_len(frame->payload_len);
    if (cap < wire_len) return PROTO_ERR_SHORT;

    buf[0] = (uint8_t)PROTO_MAGIC_0;
    buf[1] = (uint8_t)PROTO_MAGIC_1;
    buf[2] = (uint8_t)PROTO_VERSION;
    buf[3] = frame->type;
    buf[4] = frame->flags;
    buf[5] = frame->payload_len;
    put_u32le(&buf[6], frame->session_id);
    put_u32le(&buf[10], frame->seq);
    put_u32le(&buf[14], frame->ack);

    if (frame->payload_len > 0) {
        memcpy(&buf[PROTO_HEADER_LEN], frame->payload, frame->payload_len);
    }
    memset(&buf[PROTO_HEADER_LEN + frame->payload_len], 0, PROTO_AUTH_LEN);

    *out_len = wire_len;
    return PROTO_OK;
}

proto_status_t proto_frame_decode(const uint8_t *buf, size_t len,
                                  proto_frame_view_t *out) {
    size_t expected;

    if (!buf || !out) return PROTO_ERR_ARGUMENT;
    if (len < (size_t)PROTO_HEADER_LEN + (size_t)PROTO_AUTH_LEN) {
        return PROTO_ERR_SHORT;
    }
    if (buf[0] != (uint8_t)PROTO_MAGIC_0 || buf[1] != (uint8_t)PROTO_MAGIC_1) {
        return PROTO_ERR_MAGIC;
    }
    if (buf[2] != (uint8_t)PROTO_VERSION) return PROTO_ERR_VERSION;
    if (buf[3] < PROTO_TYPE_HELLO || buf[3] > PROTO_TYPE_ERROR) {
        return PROTO_ERR_TYPE;
    }
    if (buf[5] > PROTO_MAX_PAYLOAD) return PROTO_ERR_LENGTH;

    expected = proto_frame_wire_len(buf[5]);
    if (len != expected) return PROTO_ERR_LENGTH;

    out->version = buf[2];
    out->type = buf[3];
    out->flags = buf[4];
    out->session_id = get_u32le(&buf[6]);
    out->seq = get_u32le(&buf[10]);
    out->ack = get_u32le(&buf[14]);
    out->payload_len = buf[5];
    out->payload = &buf[PROTO_HEADER_LEN];
    return PROTO_OK;
}

proto_status_t proto_frame_put_crc(uint8_t *buf, size_t len) {
    uint32_t crc;
    size_t covered;

    if (!buf) return PROTO_ERR_ARGUMENT;
    if (len < (size_t)PROTO_HEADER_LEN + (size_t)PROTO_AUTH_LEN) {
        return PROTO_ERR_SHORT;
    }
    if (len != proto_frame_wire_len(buf[5])) return PROTO_ERR_LENGTH;

    covered = len - (size_t)PROTO_AUTH_LEN;
    crc = proto_crc32(buf, covered);
    put_u32le(&buf[covered], crc);
    memset(&buf[covered + PROTO_CRC_LEN], 0, PROTO_AUTH_LEN - PROTO_CRC_LEN);
    return PROTO_OK;
}

proto_status_t proto_frame_check_crc(const uint8_t *buf, size_t len) {
    proto_frame_view_t view;
    proto_status_t status;
    size_t covered;
    size_t i;

    status = proto_frame_decode(buf, len, &view);
    if (status != PROTO_OK) return status;

    covered = len - (size_t)PROTO_AUTH_LEN;
    if (proto_crc32(buf, covered) != get_u32le(&buf[covered])) {
        return PROTO_ERR_CRC;
    }
    for (i = PROTO_CRC_LEN; i < PROTO_AUTH_LEN; i++) {
        if (buf[covered + i] != 0) return PROTO_ERR_CRC;
    }
    return PROTO_OK;
}

const char *proto_status_str(proto_status_t status) {
    switch (status) {
    case PROTO_OK: return "ok";
    case PROTO_ERR_ARGUMENT: return "argument";
    case PROTO_ERR_SHORT: return "short";
    case PROTO_ERR_MAGIC: return "magic";
    case PROTO_ERR_VERSION: return "version";
    case PROTO_ERR_TYPE: return "type";
    case PROTO_ERR_LENGTH: return "length";
    case PROTO_ERR_CRC: return "crc";
    default: return "unknown";
    }
}
