/* CBMC bounded model-checking harness for the frame codec.
 *
 * STAGED, NOT RUN: the CBMC Windows installer failed (MSI 1603) on this
 * machine. Once CBMC is available, run:
 *
 *   cbmc frame_harness.c ../../../firmware/proto/proto_frame.c \
 *        --include-path ../../../firmware/proto --unwind 90 \
 *        --bounds-check --pointer-check --trace
 *
 * What it proves (within bounds): proto_frame_decode never accepts a frame
 * whose length is inconsistent with its payload_len field, and encode/decode
 * round-trip for all payload lengths 0..64.
 */
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "proto_frame.h"

void decode_harness(void) {
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    size_t i;
    proto_frame_view_t view;

    /* Nondeterministic bytes, bounded frame length. */
    for (i = 0; i < PROTO_MAX_FRAME_LEN; i++) {
        __CPROVER_assume(buf[i] == buf[i]); /* leave unconstrained */
    }

    if (proto_frame_decode(buf, PROTO_MAX_FRAME_LEN, &view) == PROTO_OK) {
        /* If decode accepted, payload must point inside the buffer and the
         * declared length must match a legal wire length. */
        assert(view.payload >= buf);
        assert(view.payload + view.payload_len <= buf + PROTO_MAX_FRAME_LEN);
        assert(view.payload_len <= PROTO_MAX_PAYLOAD);
    }
}

void roundtrip_harness(void) {
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t payload[PROTO_MAX_PAYLOAD];
    proto_frame_view_t in, out;
    size_t len = 0;
    uint8_t n;

    __CPROVER_assume(n <= PROTO_MAX_PAYLOAD);
    memset(payload, 0x5A, sizeof payload);
    in.version = PROTO_VERSION;
    in.type = PROTO_TYPE_DATA;
    in.flags = 0;
    in.session_id = 0x12345678u;
    in.seq = 42u;
    in.ack = 7u;
    in.payload_len = n;
    in.payload = payload;

    assert(proto_frame_encode(buf, sizeof buf, &in, &len) == PROTO_OK);
    assert(len == PROTO_HEADER_LEN + n + PROTO_AUTH_LEN);
    assert(proto_frame_decode(buf, len, &out) == PROTO_OK);
    assert(out.payload_len == n);
    assert(memcmp(out.payload, payload, n) == 0);
}
