#include <stdio.h>
#include <string.h>

#include "proto_frame.h"

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static uint32_t rng_state = 0x12345678u;

static uint32_t rng_next(void) {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

static void fill_payload(uint8_t *payload, size_t len, uint32_t seed) {
    size_t i;
    for (i = 0; i < len; i++) {
        payload[i] = (uint8_t)(seed + (uint32_t)i * 31u);
    }
}

static void init_frame(proto_frame_view_t *frame, uint8_t type,
                       const uint8_t *payload, uint8_t payload_len) {
    frame->version = (uint8_t)PROTO_VERSION;
    frame->type = type;
    frame->flags = PROTO_FLAG_ACK_REQ;
    frame->session_id = 0x11223344u;
    frame->seq = 0xAABBCCDDu;
    frame->ack = 0x01020304u;
    frame->payload_len = payload_len;
    frame->payload = payload;
}

static void test_crc_vector(void) {
    static const uint8_t text[] = "123456789";
    CHECK(proto_crc32(text, 9) == 0xCBF43926u);
}

static void test_round_trip_all_lengths(void) {
    uint8_t payload[PROTO_MAX_PAYLOAD];
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    unsigned len;

    for (len = 0; len <= PROTO_MAX_PAYLOAD; len++) {
        proto_frame_view_t in;
        proto_frame_view_t out;
        size_t out_len = 0;

        fill_payload(payload, len, 0xC0FFEEu + len);
        init_frame(&in, PROTO_TYPE_DATA, payload, (uint8_t)len);

        CHECK(proto_frame_encode(buf, sizeof buf, &in, &out_len) == PROTO_OK);
        CHECK(out_len == (size_t)PROTO_HEADER_LEN + len + PROTO_AUTH_LEN);
        CHECK(proto_frame_wire_len((uint8_t)len) == out_len);
        CHECK(proto_frame_put_crc(buf, out_len) == PROTO_OK);
        CHECK(proto_frame_check_crc(buf, out_len) == PROTO_OK);

        memset(&out, 0, sizeof out);
        CHECK(proto_frame_decode(buf, out_len, &out) == PROTO_OK);
        CHECK(out.version == PROTO_VERSION);
        CHECK(out.type == PROTO_TYPE_DATA);
        CHECK(out.flags == PROTO_FLAG_ACK_REQ);
        CHECK(out.session_id == in.session_id);
        CHECK(out.seq == in.seq);
        CHECK(out.ack == in.ack);
        CHECK(out.payload_len == len);
        CHECK(memcmp(out.payload, payload, len) == 0);
    }
}

static void test_little_endian_fields(void) {
    uint8_t payload[1] = { 0x5A };
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    proto_frame_view_t in;
    size_t out_len = 0;

    init_frame(&in, PROTO_TYPE_PING, payload, 1);
    in.session_id = 0x11223344u;
    in.seq = 0xAABBCCDDu;
    in.ack = 0x01020304u;

    CHECK(proto_frame_encode(buf, sizeof buf, &in, &out_len) == PROTO_OK);
    CHECK(buf[0] == 'B');
    CHECK(buf[1] == 'C');
    CHECK(buf[2] == PROTO_VERSION);
    CHECK(buf[3] == PROTO_TYPE_PING);
    CHECK(buf[6] == 0x44 && buf[7] == 0x33 && buf[8] == 0x22 && buf[9] == 0x11);
    CHECK(buf[10] == 0xDD && buf[11] == 0xCC && buf[12] == 0xBB && buf[13] == 0xAA);
    CHECK(buf[14] == 0x04 && buf[15] == 0x03 && buf[16] == 0x02 && buf[17] == 0x01);
    CHECK(buf[18] == 0x5A);
}

static void test_every_prefix_rejected(void) {
    uint8_t payload[PROTO_MAX_PAYLOAD];
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    proto_frame_view_t in;
    size_t full_len = 0;
    size_t cut;

    fill_payload(payload, sizeof payload, 7);
    init_frame(&in, PROTO_TYPE_HELLO_ACK, payload, PROTO_MAX_PAYLOAD);
    CHECK(proto_frame_encode(buf, sizeof buf, &in, &full_len) == PROTO_OK);
    CHECK(proto_frame_put_crc(buf, full_len) == PROTO_OK);

    for (cut = 0; cut < full_len; cut++) {
        proto_frame_view_t out;
        CHECK(proto_frame_decode(buf, cut, &out) != PROTO_OK);
    }
}

static void test_bad_fields(void) {
    uint8_t payload[4] = { 1, 2, 3, 4 };
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t copy[PROTO_MAX_FRAME_LEN];
    proto_frame_view_t in;
    proto_frame_view_t out;
    size_t len = 0;

    init_frame(&in, PROTO_TYPE_DATA, payload, sizeof payload);
    CHECK(proto_frame_encode(buf, sizeof buf, &in, &len) == PROTO_OK);
    CHECK(proto_frame_put_crc(buf, len) == PROTO_OK);

    memcpy(copy, buf, len);
    copy[0] = 'X';
    CHECK(proto_frame_decode(copy, len, &out) == PROTO_ERR_MAGIC);

    memcpy(copy, buf, len);
    copy[1] = 'X';
    CHECK(proto_frame_decode(copy, len, &out) == PROTO_ERR_MAGIC);

    memcpy(copy, buf, len);
    copy[2] = PROTO_VERSION + 1;
    CHECK(proto_frame_decode(copy, len, &out) == PROTO_ERR_VERSION);

    memcpy(copy, buf, len);
    copy[3] = 0;
    CHECK(proto_frame_decode(copy, len, &out) == PROTO_ERR_TYPE);

    memcpy(copy, buf, len);
    copy[3] = PROTO_TYPE_ERROR + 1;
    CHECK(proto_frame_decode(copy, len, &out) == PROTO_ERR_TYPE);

    memcpy(copy, buf, len);
    copy[5] = PROTO_MAX_PAYLOAD + 1;
    CHECK(proto_frame_decode(copy, len, &out) == PROTO_ERR_LENGTH);

    memcpy(copy, buf, len);
    copy[5] = 3;
    CHECK(proto_frame_decode(copy, len, &out) == PROTO_ERR_LENGTH);

    CHECK(proto_frame_decode(buf, len - 1, &out) == PROTO_ERR_LENGTH);
    CHECK(proto_frame_decode(buf, len + 1, &out) == PROTO_ERR_LENGTH);
    CHECK(proto_frame_decode(buf, 25, &out) == PROTO_ERR_SHORT);
    CHECK(proto_frame_decode(NULL, len, &out) == PROTO_ERR_ARGUMENT);
    CHECK(proto_frame_decode(buf, len, NULL) == PROTO_ERR_ARGUMENT);
}

static void test_crc_rejects_tamper(void) {
    uint8_t payload[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t copy[PROTO_MAX_FRAME_LEN];
    proto_frame_view_t in;
    size_t len = 0;
    size_t i;
    size_t covered;

    init_frame(&in, PROTO_TYPE_DATA, payload, sizeof payload);
    CHECK(proto_frame_encode(buf, sizeof buf, &in, &len) == PROTO_OK);
    CHECK(proto_frame_put_crc(buf, len) == PROTO_OK);
    CHECK(proto_frame_check_crc(buf, len) == PROTO_OK);

    covered = len - PROTO_AUTH_LEN;

    /* Any single-bit flip in the covered region is rejected. Structural
     * fields (magic, version, length) report their own status before the CRC
     * is examined, so only require rejection here. */
    for (i = 0; i < covered; i++) {
        memcpy(copy, buf, len);
        copy[i] ^= 0x01;
        CHECK(proto_frame_check_crc(copy, len) != PROTO_OK);
    }

    /* Outside those structural fields, corruption must be reported as CRC. */
    for (i = 4; i < covered; i++) {
        if (i == 5) continue; /* payload_len */
        memcpy(copy, buf, len);
        copy[i] ^= 0x01;
        CHECK(proto_frame_check_crc(copy, len) == PROTO_ERR_CRC);
    }

    for (i = 0; i < PROTO_AUTH_LEN; i++) {
        memcpy(copy, buf, len);
        copy[covered + i] ^= 0x80;
        CHECK(proto_frame_check_crc(copy, len) == PROTO_ERR_CRC);
    }
}

static void test_encode_bounds(void) {
    uint8_t payload[PROTO_MAX_PAYLOAD];
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    proto_frame_view_t in;
    size_t out_len = 0;

    fill_payload(payload, sizeof payload, 3);
    init_frame(&in, PROTO_TYPE_DATA, payload, 4);

    CHECK(proto_frame_encode(NULL, sizeof buf, &in, &out_len) == PROTO_ERR_ARGUMENT);
    CHECK(proto_frame_encode(buf, sizeof buf, NULL, &out_len) == PROTO_ERR_ARGUMENT);
    CHECK(proto_frame_encode(buf, sizeof buf, &in, NULL) == PROTO_ERR_ARGUMENT);
    CHECK(proto_frame_encode(buf, PROTO_HEADER_LEN + 4 + PROTO_AUTH_LEN - 1,
                             &in, &out_len) == PROTO_ERR_SHORT);

    in.payload = NULL;
    CHECK(proto_frame_encode(buf, sizeof buf, &in, &out_len) == PROTO_ERR_ARGUMENT);

    in.payload = payload;
    in.payload_len = PROTO_MAX_PAYLOAD + 1;
    CHECK(proto_frame_encode(buf, sizeof buf, &in, &out_len) == PROTO_ERR_LENGTH);

    in.payload_len = 0;
    in.type = 0;
    CHECK(proto_frame_encode(buf, sizeof buf, &in, &out_len) == PROTO_ERR_TYPE);

    CHECK(proto_frame_wire_len(PROTO_MAX_PAYLOAD + 1) == 0);
    CHECK(proto_frame_put_crc(NULL, 0) == PROTO_ERR_ARGUMENT);
    CHECK(proto_frame_put_crc(buf, 0) == PROTO_ERR_SHORT);
}

static void test_fuzz_parser(void) {
    uint8_t buf[160];
    int iteration;

    for (iteration = 0; iteration < 100000; iteration++) {
        size_t len = (size_t)(rng_next() % (uint32_t)sizeof buf);
        size_t i;
        proto_frame_view_t out;
        proto_status_t status;

        for (i = 0; i < len; i++) {
            buf[i] = (uint8_t)rng_next();
        }

        status = proto_frame_decode(buf, len, &out);
        if (status == PROTO_OK) {
            CHECK(proto_frame_wire_len(out.payload_len) == len);
        }
        (void)proto_frame_check_crc(buf, len);
    }
}

int main(void) {
    test_crc_vector();
    test_round_trip_all_lengths();
    test_little_endian_fields();
    test_every_prefix_rejected();
    test_bad_fields();
    test_crc_rejects_tamper();
    test_encode_bounds();
    test_fuzz_parser();

    if (failures) {
        printf("frame: %d failure(s)\n", failures);
        return 1;
    }
    printf("frame: PASS\n");
    return 0;
}
