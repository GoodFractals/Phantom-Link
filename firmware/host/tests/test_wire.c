#include <stdio.h>
#include <string.h>

#include "proto_frame.h"
#include "proto_session.h"

/*
 * A host harness for the receive path: frame decode, CRC, session ID, and the
 * replay window together. It mirrors what main.c does before the app sees a
 * datagram, without needing a board or the Pico SDK.
 */

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static size_t make_frame(uint8_t *buf, size_t cap, uint32_t session_id,
                         uint32_t seq, uint8_t payload_byte) {
    proto_frame_view_t frame;
    size_t len = 0;

    frame.version = (uint8_t)PROTO_VERSION;
    frame.type = PROTO_TYPE_DATA;
    frame.flags = 0;
    frame.session_id = session_id;
    frame.seq = seq;
    frame.ack = 0;
    frame.payload_len = 1;
    frame.payload = &payload_byte;

    CHECK(proto_frame_encode(buf, cap, &frame, &len) == PROTO_OK);
    CHECK(proto_frame_put_crc(buf, len) == PROTO_OK);
    return len;
}

static proto_status_t receive(const uint8_t *buf, size_t len,
                              proto_session_t *session) {
    proto_status_t status = proto_frame_check_crc(buf, len);
    proto_frame_view_t frame;

    if (status != PROTO_OK) return status;
    status = proto_frame_decode(buf, len, &frame);
    if (status != PROTO_OK) return status;

    (void)proto_session_accept(session, frame.session_id, frame.seq);
    return PROTO_OK;
}

static void test_replay_over_wire(void) {
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    proto_session_t session;
    size_t len;
    int i;
    int attempt;

    len = make_frame(buf, sizeof buf, 0xABCD1234u, 7, 3);
    proto_session_init(&session, 0xABCD1234u);

    CHECK(receive(buf, len, &session) == PROTO_OK);
    CHECK(session.accepted == 1);

    /* The same bytes, replayed 1000 times, are decoded and then dropped. */
    for (attempt = 0; attempt < 1000; attempt++) {
        CHECK(receive(buf, len, &session) == PROTO_OK);
    }
    CHECK(session.accepted == 1);
    CHECK(session.duplicates == 1000);

    /* A corrupted payload never reaches the session layer. */
    buf[PROTO_HEADER_LEN] ^= 0x01;
    CHECK(receive(buf, len, &session) == PROTO_ERR_CRC);
    CHECK(session.accepted == 1);
    CHECK(session.duplicates == 1000);
    buf[PROTO_HEADER_LEN] ^= 0x01;

    /* A frame from a different session is rejected by the session gate. */
    len = make_frame(buf, sizeof buf, 0xABCD1235u, 8, 4);
    CHECK(receive(buf, len, &session) == PROTO_OK);
    CHECK(session.wrong_session == 1);
    CHECK(session.accepted == 1);

    /* Inside the window, a later frame followed by the gap fill is accepted
     * once each and nothing is duplicated. */
    len = make_frame(buf, sizeof buf, 0xABCD1234u, 9, 5);
    CHECK(receive(buf, len, &session) == PROTO_OK);
    len = make_frame(buf, sizeof buf, 0xABCD1234u, 8, 4);
    CHECK(receive(buf, len, &session) == PROTO_OK);
    CHECK(session.accepted == 3);

    for (i = 0; i < 100; i++) {
        len = make_frame(buf, sizeof buf, 0xABCD1234u, 8, 4);
        CHECK(receive(buf, len, &session) == PROTO_OK);
    }
    CHECK(session.accepted == 3);
    CHECK(session.duplicates == 1100);
}

static void test_new_session_resets_window(void) {
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    proto_session_t session;
    size_t len;

    proto_session_init(&session, 1);
    len = make_frame(buf, sizeof buf, 1, 0, 0);
    CHECK(receive(buf, len, &session) == PROTO_OK);
    CHECK(session.accepted == 1);

    proto_session_init(&session, 2);
    len = make_frame(buf, sizeof buf, 2, 0, 0);
    CHECK(receive(buf, len, &session) == PROTO_OK);
    CHECK(session.accepted == 1);
    CHECK(session.duplicates == 0);

    /* The old session's frame is not mistaken for the new one. */
    len = make_frame(buf, sizeof buf, 1, 1, 0);
    CHECK(receive(buf, len, &session) == PROTO_OK);
    CHECK(session.wrong_session == 1);
    CHECK(session.accepted == 1);
}

int main(void) {
    test_replay_over_wire();
    test_new_session_resets_window();

    if (failures) {
        printf("wire: %d failure(s)\n", failures);
        return 1;
    }
    printf("wire: PASS\n");
    return 0;
}
