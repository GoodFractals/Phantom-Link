#include <stdio.h>

#include "proto_session.h"

/*
 * Two session objects, one per board, driven through the handshake and a
 * heartbeat exchange. This is the sequence contract main.c relies on:
 * each direction starts at 0, HELLO adopts the initiator's ID, and an ACK
 * is accepted once and only once.
 */

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static void test_handshake_and_heartbeat(void) {
    proto_session_t sta;
    proto_session_t ap;
    uint32_t session_id = 0xA1B2C3D4u;
    uint32_t hello_seq;
    uint32_t ack_seq;
    uint32_t ping_seq;
    uint32_t pong_seq;

    /* The AP has no session until HELLO arrives. */
    proto_session_init(&ap, 0);
    proto_session_init(&sta, session_id);
    CHECK(sta.session_id == session_id);
    CHECK(!proto_session_active(&ap));

    /* STA -> HELLO(seq 0). The AP adopts the ID, then accepts seq 0. */
    hello_seq = proto_session_next_seq(&sta);
    CHECK(hello_seq == 0);

    proto_session_init(&ap, session_id);
    CHECK(proto_session_accept(&ap, session_id, hello_seq) == PROTO_RX_ACCEPTED);

    /* AP -> HELLO_ACK(seq 0). The STA accepts it once. */
    ack_seq = proto_session_next_seq(&ap);
    CHECK(ack_seq == 0);
    CHECK(proto_session_accept(&sta, session_id, ack_seq) == PROTO_RX_ACCEPTED);
    CHECK(proto_session_accept(&sta, session_id, ack_seq) ==
          PROTO_RX_DUPLICATE);

    /* STA heartbeat: STA seq 1, AP replies with AP seq 1. */
    ping_seq = proto_session_next_seq(&sta);
    CHECK(ping_seq == 1);
    CHECK(proto_session_accept(&ap, session_id, ping_seq) == PROTO_RX_ACCEPTED);

    pong_seq = proto_session_next_seq(&ap);
    CHECK(pong_seq == 1);
    CHECK(proto_session_accept(&sta, session_id, pong_seq) == PROTO_RX_ACCEPTED);

    /* AP heartbeat: AP seq 2, STA replies with STA seq 2. */
    ping_seq = proto_session_next_seq(&ap);
    CHECK(ping_seq == 2);
    CHECK(proto_session_accept(&sta, session_id, ping_seq) == PROTO_RX_ACCEPTED);

    pong_seq = proto_session_next_seq(&sta);
    CHECK(pong_seq == 2);
    CHECK(proto_session_accept(&ap, session_id, pong_seq) == PROTO_RX_ACCEPTED);

    /* A stale HELLO inside the live session is a duplicate, not a reset. */
    CHECK(proto_session_accept(&ap, session_id, hello_seq) ==
          PROTO_RX_DUPLICATE);
    CHECK(ap.duplicates == 1);

    /* A new session ID starts clean on both sides. */
    proto_session_init(&sta, 0x01020304u);
    hello_seq = proto_session_next_seq(&sta);
    proto_session_init(&ap, 0x01020304u);
    CHECK(proto_session_accept(&ap, 0x01020304u, hello_seq) ==
          PROTO_RX_ACCEPTED);
    CHECK(proto_session_accept(&ap, session_id, 3) == PROTO_RX_WRONG_SESSION);
}

int main(void) {
    test_handshake_and_heartbeat();

    if (failures) {
        printf("handshake: %d failure(s)\n", failures);
        return 1;
    }
    printf("handshake: PASS\n");
    return 0;
}
