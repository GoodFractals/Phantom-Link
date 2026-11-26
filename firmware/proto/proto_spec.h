#ifndef PHANTOM_PROTO_SPEC_H
#define PHANTOM_PROTO_SPEC_H

/*
 * Wire constants for the Bonnie & Clyde protocol.
 *
 * This header is the single place those numbers are defined. It has no Pico
 * SDK dependency so the host tests and the firmware share exactly one spec.
 *
 * Frame layout (v1):
 *
 *   off  size  field
 *    0    2    magic 'B','C'
 *    2    1    version
 *    3    1    type
 *    4    1    flags
 *    5    1    payload_len (0..64)
 *    6    4    session_id
 *   10    4    seq
 *   14    4    ack
 *   18    n    payload
 *  18+n   8    auth field
 *
 * Phase 1 fills the auth field with CRC32 in the first four bytes and four
 * reserved zero bytes. Phase 2 replaces it with a truncated CMAC of the same
 * length, so the wire size does not change.
 */

#include <stddef.h>
#include <stdint.h>

#define PROTO_MAGIC_0 'B'
#define PROTO_MAGIC_1 'C'
#define PROTO_VERSION 1u

#define PROTO_HEADER_LEN 18u
#define PROTO_MAX_PAYLOAD 64u
#define PROTO_AUTH_LEN 8u
#define PROTO_CRC_LEN 4u
#define PROTO_MAX_FRAME_LEN (PROTO_HEADER_LEN + PROTO_MAX_PAYLOAD + PROTO_AUTH_LEN)

#define PROTO_FLAG_ACK_REQ   (1u << 0)
#define PROTO_FLAG_ENCRYPTED (1u << 1)
#define PROTO_FLAG_FINAL     (1u << 2)
#define PROTO_FLAG_SFO_SYNC  (1u << 3)
/* Ticket 34: header fields (bytes 6..17) are CTR-masked with an ECDH-derived
 * key when this bit is set, otherwise with the static PSK (HELLO/HELLO_ACK and
 * any frame sent before session keys exist). The bit lives in the clear prefix
 * so the receiver can select the unmask key before decoding the header. */
#define PROTO_FLAG_EPH_MASK  (1u << 4)

typedef enum {
    PROTO_TYPE_HELLO = 1,
    PROTO_TYPE_HELLO_ACK = 2,
    PROTO_TYPE_DATA = 3,
    PROTO_TYPE_ACK = 4,
    PROTO_TYPE_PING = 5,
    PROTO_TYPE_PONG = 6,
    PROTO_TYPE_BYE = 7,
    PROTO_TYPE_SFO = 8,
    PROTO_TYPE_ERROR = 9
} proto_type_t;

#endif
