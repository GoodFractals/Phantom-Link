#include <stdio.h>

#include "proto_spec.h"

/*
 * The wire spec is a compile-time contract. If any of these drift, the host
 * build fails and no firmware is produced.
 */
_Static_assert(PROTO_MAGIC_0 == 'B', "magic byte 0 must be 'B'");
_Static_assert(PROTO_MAGIC_1 == 'C', "magic byte 1 must be 'C'");
_Static_assert(PROTO_VERSION == 1u, "protocol version must be 1");

_Static_assert(PROTO_HEADER_LEN == 18u, "header must be 18 bytes");
_Static_assert(PROTO_MAX_PAYLOAD == 64u, "payload must cap at 64 bytes");
_Static_assert(PROTO_AUTH_LEN == 8u, "auth field must be 8 bytes");
_Static_assert(PROTO_MAX_FRAME_LEN == 90u, "frame must cap at 90 bytes");

_Static_assert(PROTO_FLAG_ACK_REQ == 0x01u, "ACK_REQ flag changed");
_Static_assert(PROTO_FLAG_ENCRYPTED == 0x02u, "ENCRYPTED flag changed");
_Static_assert(PROTO_FLAG_FINAL == 0x04u, "FINAL flag changed");
_Static_assert(PROTO_FLAG_SFO_SYNC == 0x08u, "SFO_SYNC flag changed");

_Static_assert(PROTO_TYPE_HELLO == 1, "HELLO type changed");
_Static_assert(PROTO_TYPE_HELLO_ACK == 2, "HELLO_ACK type changed");
_Static_assert(PROTO_TYPE_DATA == 3, "DATA type changed");
_Static_assert(PROTO_TYPE_ACK == 4, "ACK type changed");
_Static_assert(PROTO_TYPE_PING == 5, "PING type changed");
_Static_assert(PROTO_TYPE_PONG == 6, "PONG type changed");
_Static_assert(PROTO_TYPE_BYE == 7, "BYE type changed");
_Static_assert(PROTO_TYPE_SFO == 8, "SFO type changed");
_Static_assert(PROTO_TYPE_ERROR == 9, "ERROR type changed");

int main(void) {
    printf("spec: PASS\n");
    return 0;
}
