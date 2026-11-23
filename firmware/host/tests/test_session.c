#include <stdio.h>

#include "proto_session.h"

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static void test_init_and_tx_sequence(void) {
    proto_session_t s;

    proto_session_init(&s, 0xDEADBEEFu);
    CHECK(s.session_id == 0xDEADBEEFu);
    CHECK(proto_session_active(&s));
    CHECK(proto_session_next_seq(&s) == 0);
    CHECK(proto_session_next_seq(&s) == 1);
    CHECK(proto_session_next_seq(&s) == 2);
    CHECK(s.tx_seq == 3);

    proto_session_init(&s, 0x01020304u);
    CHECK(proto_session_next_seq(&s) == 0);
    CHECK(s.accepted == 0);
    CHECK(s.duplicates == 0);
    CHECK(s.stale == 0);
    CHECK(s.wrong_session == 0);
}

static void test_in_order_and_duplicates(void) {
    proto_session_t s;
    uint32_t seq;

    proto_session_init(&s, 7);
    for (seq = 0; seq < 1000; seq++) {
        CHECK(proto_session_accept(&s, 7, seq) == PROTO_RX_ACCEPTED);
    }
    CHECK(s.accepted == 1000);
    CHECK(s.duplicates == 0);

    for (seq = 968; seq <= 999; seq++) {
        CHECK(proto_session_accept(&s, 7, seq) == PROTO_RX_DUPLICATE);
    }
    CHECK(s.duplicates == 32);
    CHECK(s.accepted == 1000);
}

static void test_out_of_order_window(void) {
    proto_session_t s;
    static const uint32_t order[] = { 5, 3, 4, 0, 2, 1 };

    proto_session_init(&s, 11);
    for (size_t i = 0; i < sizeof order / sizeof order[0]; i++) {
        CHECK(proto_session_accept(&s, 11, order[i]) == PROTO_RX_ACCEPTED);
    }
    CHECK(s.accepted == 6);
    CHECK(s.rx_max == 5);

    for (size_t i = 0; i < sizeof order / sizeof order[0]; i++) {
        CHECK(proto_session_accept(&s, 11, order[i]) == PROTO_RX_DUPLICATE);
    }
    CHECK(s.duplicates == 6);
}

static void test_window_edges(void) {
    proto_session_t s;

    proto_session_init(&s, 21);
    CHECK(proto_session_accept(&s, 21, 100) == PROTO_RX_ACCEPTED);

    /* 69 is 31 below the max: still inside the 32-bit window. */
    CHECK(proto_session_accept(&s, 21, 69) == PROTO_RX_ACCEPTED);
    CHECK(proto_session_accept(&s, 21, 69) == PROTO_RX_DUPLICATE);

    /* 68 is 32 below the max: stale, even though its bit is clear. */
    CHECK(proto_session_accept(&s, 21, 68) == PROTO_RX_STALE);
    CHECK(s.stale == 1);

    /* A jump clears the bitmap and accepts the far sequence. */
    CHECK(proto_session_accept(&s, 21, 200) == PROTO_RX_ACCEPTED);
    CHECK(proto_session_accept(&s, 21, 168) == PROTO_RX_STALE);
    CHECK(proto_session_accept(&s, 21, 169) == PROTO_RX_ACCEPTED);
}

static void test_ten_thousand_replays(void) {
    proto_session_t s;
    int i;

    proto_session_init(&s, 33);
    CHECK(proto_session_accept(&s, 33, 500) == PROTO_RX_ACCEPTED);

    for (i = 0; i < 10000; i++) {
        CHECK(proto_session_accept(&s, 33, 500) == PROTO_RX_DUPLICATE);
    }
    CHECK(s.accepted == 1);
    CHECK(s.duplicates == 10000);
    CHECK(s.stale == 0);

    /* Exactly 32 below the max is outside the window. */
    CHECK(proto_session_accept(&s, 33, 468) == PROTO_RX_STALE);
    CHECK(s.stale == 1);
}

static void test_wrong_session_and_reset(void) {
    proto_session_t s;

    proto_session_init(&s, 44);
    CHECK(proto_session_accept(&s, 44, 10) == PROTO_RX_ACCEPTED);
    CHECK(proto_session_accept(&s, 45, 11) == PROTO_RX_WRONG_SESSION);
    CHECK(s.wrong_session == 1);
    CHECK(s.rx_max == 10);

    /* A new session starts from a clean slate and does not inherit the old
     * window, sequence, or counters. */
    proto_session_init(&s, 55);
    CHECK(proto_session_accept(&s, 44, 11) == PROTO_RX_WRONG_SESSION);
    CHECK(proto_session_accept(&s, 55, 0) == PROTO_RX_ACCEPTED);
    CHECK(s.accepted == 1);
    CHECK(s.duplicates == 0);
    CHECK(s.stale == 0);
    CHECK(s.rx_max == 0);

    proto_session_init(&s, 0);
    CHECK(!proto_session_active(&s));
}

int main(void) {
    test_init_and_tx_sequence();
    test_in_order_and_duplicates();
    test_out_of_order_window();
    test_window_edges();
    test_ten_thousand_replays();
    test_wrong_session_and_reset();

    if (failures) {
        printf("session: %d failure(s)\n", failures);
        return 1;
    }
    printf("session: PASS\n");
    return 0;
}
