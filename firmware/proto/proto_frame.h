#ifndef PHANTOM_PROTO_FRAME_H
#define PHANTOM_PROTO_FRAME_H

/*
 * Frame encode/decode for protocol v1.
 *
 * The codec validates structure only: magic, version, type, payload length,
 * and exact buffer length. It never allocates and never copies the payload;
 * proto_frame_view_t.payload points into the caller's buffer.
 *
 * Authentication is a separate step so the same wire format can carry a CRC32
 * during the plaintext phase and a truncated CMAC once crypto lands:
 *
 *   proto_frame_put_crc(frame, len);    // phase 1
 *   proto_frame_check_crc(frame, len);  // phase 1
 *
 * The CRC occupies the first four bytes of the eight-byte auth field and the
 * remaining four bytes are zero. Phase 2 treats the whole field as a CMAC.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "proto_spec.h"

typedef enum {
    PROTO_OK = 0,
    PROTO_ERR_ARGUMENT,
    PROTO_ERR_SHORT,
    PROTO_ERR_MAGIC,
    PROTO_ERR_VERSION,
    PROTO_ERR_TYPE,
    PROTO_ERR_LENGTH,
    PROTO_ERR_CRC
} proto_status_t;

typedef struct {
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint32_t session_id;
    uint32_t seq;
    uint32_t ack;
    uint8_t payload_len;
    const uint8_t *payload; /* borrowed from the decoded buffer */
} proto_frame_view_t;

/* Returns 0 when payload_len exceeds PROTO_MAX_PAYLOAD. */
size_t proto_frame_wire_len(uint8_t payload_len);

/*
 * Writes a complete frame, including zeroed auth bytes. The caller must call
 * proto_frame_put_crc (or, later, the crypto layer) before sending.
 */
proto_status_t proto_frame_encode(uint8_t *buf, size_t cap,
                                  const proto_frame_view_t *frame,
                                  size_t *out_len);

proto_status_t proto_frame_decode(const uint8_t *buf, size_t len,
                                  proto_frame_view_t *out);

/* Fills the auth field with CRC32(header || payload) and four zero bytes. */
proto_status_t proto_frame_put_crc(uint8_t *buf, size_t len);

/* Verifies the structure, the CRC, and that the reserved bytes are zero. */
proto_status_t proto_frame_check_crc(const uint8_t *buf, size_t len);

/* IEEE 802.3 CRC32, exposed for tests and for the session layer's counters. */
uint32_t proto_crc32(const uint8_t *data, size_t len);

const char *proto_status_str(proto_status_t status);

#endif
