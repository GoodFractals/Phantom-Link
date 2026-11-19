#include <stdio.h>
#include <string.h>

#include "proto_msg.h"

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static const char ALPHABET[] =
    " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789=+-.,?!()[]\"";
#define ALPHABET_LEN ((uint8_t)(sizeof ALPHABET - 1u))

static uint8_t code_of(char c) {
    const char *p = strchr(ALPHABET, c);
    return p ? (uint8_t)(p - ALPHABET) : 0;
}

static uint8_t symbol_at(const char *text, uint8_t pos) {
    unsigned divisor = (pos % 3u == 0) ? 36u : (pos % 3u == 1) ? 6u : 1u;
    return (uint8_t)(((unsigned)code_of(text[pos / 3u]) / divisor) % 6u);
}

static void feed_symbol(proto_msg_rx_t *rx, uint16_t id, const char *text,
                        uint8_t pos) {
    (void)proto_msg_rx_frame(rx, id, pos, symbol_at(text, pos), false);
}

static proto_msg_event_t feed_all(proto_msg_rx_t *rx, uint16_t id,
                                  const char *text) {
    uint8_t len = (uint8_t)strlen(text);
    uint8_t pos;

    for (pos = 0; pos < (uint8_t)(len * 3u); pos++) {
        feed_symbol(rx, id, text, pos);
    }
    return proto_msg_rx_frame(rx, id, (uint8_t)(len * 3u), 0, true);
}

static void test_rx_in_order_and_duplicate_final(void) {
    proto_msg_rx_t rx;

    proto_msg_rx_init(&rx, ALPHABET, ALPHABET_LEN);
    CHECK(feed_all(&rx, 1, "HELLO") == PROTO_MSG_EV_COMPLETE);
    CHECK(rx.completed == 1);
    CHECK(rx.have_completed);
    CHECK(rx.completed_id == 1);
    CHECK(strcmp(proto_msg_rx_text(&rx), "HELLO") == 0);
    CHECK(proto_msg_rx_len(&rx) == 5);
    CHECK(proto_msg_rx_next_pos(&rx, 1) == 15);

    /* A final retried after a lost ACK is not delivered twice. */
    CHECK(proto_msg_rx_frame(&rx, 1, 15, 0, true) == PROTO_MSG_EV_DUPLICATE);
    CHECK(rx.completed == 1);
    CHECK(rx.duplicates == 1);
    CHECK(strcmp(proto_msg_rx_text(&rx), "HELLO") == 0);
}

static void test_rx_gap_and_resume(void) {
    proto_msg_rx_t rx;
    uint8_t pos;

    proto_msg_rx_init(&rx, ALPHABET, ALPHABET_LEN);

    /* "WORLD" but position 6 is lost. */
    for (pos = 0; pos < 6; pos++) feed_symbol(&rx, 7, "WORLD", pos);
    for (pos = 7; pos < 15; pos++) feed_symbol(&rx, 7, "WORLD", pos);
    CHECK(rx.symbol_count == 6);
    CHECK(proto_msg_rx_next_pos(&rx, 7) == 6);

    /* The sender resumes at the first hole; the out-of-order tail is ignored
     * until then and is not appended out of order. */
    for (pos = 6; pos < 15; pos++) feed_symbol(&rx, 7, "WORLD", pos);
    CHECK(rx.symbol_count == 15);
    CHECK(proto_msg_rx_frame(&rx, 7, 15, 0, true) == PROTO_MSG_EV_COMPLETE);
    CHECK(strcmp(proto_msg_rx_text(&rx), "WORLD") == 0);
    CHECK(rx.completed == 1);
}

static void test_rx_whole_message_retry(void) {
    proto_msg_rx_t rx;
    uint8_t pos;

    proto_msg_rx_init(&rx, ALPHABET, ALPHABET_LEN);

    /* The first run loses its tail, so the sender retries from 0. Positions
     * already held are ignored, not appended a second time. */
    for (pos = 0; pos < 6; pos++) feed_symbol(&rx, 9, "CAT", pos);
    for (pos = 0; pos < 9; pos++) feed_symbol(&rx, 9, "CAT", pos);
    CHECK(rx.symbol_count == 9);
    CHECK(rx.duplicates > 0);
    CHECK(proto_msg_rx_frame(&rx, 9, 9, 0, true) == PROTO_MSG_EV_COMPLETE);
    CHECK(strcmp(proto_msg_rx_text(&rx), "CAT") == 0);
    CHECK(rx.completed == 1);
}

static void test_rx_new_id_preempts_and_bounds(void) {
    proto_msg_rx_t rx;
    uint8_t pos;

    proto_msg_rx_init(&rx, ALPHABET, ALPHABET_LEN);

    for (pos = 0; pos < 3; pos++) feed_symbol(&rx, 1, "ABC", pos);
    CHECK(rx.active);

    /* A new ID wins; the abandoned message never completes. */
    CHECK(feed_all(&rx, 2, "XY") == PROTO_MSG_EV_COMPLETE);
    CHECK(strcmp(proto_msg_rx_text(&rx), "XY") == 0);
    CHECK(rx.dropped > 0);
    CHECK(rx.completed == 1);

    /* An unknown position with no active message is a gap, not a start. */
    CHECK(proto_msg_rx_frame(&rx, 3, 9, 0, false) == PROTO_MSG_EV_IGNORED);
    CHECK(!rx.active);
}

static void test_rx_bad_symbols(void) {
    proto_msg_rx_t rx;

    proto_msg_rx_init(&rx, ALPHABET, ALPHABET_LEN);
    CHECK(proto_msg_rx_frame(&rx, 4, 0, 6, false) == PROTO_MSG_EV_BAD);
    CHECK(rx.completed == 0);

    /* 5,5,5 is 215, outside the 49-code alphabet. */
    proto_msg_rx_reset(&rx);
    CHECK(proto_msg_rx_frame(&rx, 4, 0, 5, false) == PROTO_MSG_EV_SYMBOL);
    CHECK(proto_msg_rx_frame(&rx, 4, 1, 5, false) == PROTO_MSG_EV_SYMBOL);
    CHECK(proto_msg_rx_frame(&rx, 4, 2, 5, false) == PROTO_MSG_EV_BAD);
    CHECK(!rx.active);
    CHECK(rx.completed == 0);
}

static void test_tx_symbols(void) {
    proto_msg_tx_t tx;
    uint8_t pos;

    proto_msg_tx_init(&tx, ALPHABET, ALPHABET_LEN);
    CHECK(proto_msg_tx_begin(&tx, "AZ?", 3));
    CHECK(tx.phase == PROTO_TX_SENDING);
    CHECK(tx.msg_id != 0);
    CHECK(tx.total_symbols == 9);

    for (pos = 0; pos < 9; pos++) {
        tx.pos = pos;
        CHECK(proto_msg_tx_symbol(&tx) == symbol_at("AZ?", pos));
    }
}

static void test_tx_partial_ack_and_retry(void) {
    proto_msg_tx_t tx;
    uint16_t id;

    proto_msg_tx_init(&tx, ALPHABET, ALPHABET_LEN);
    CHECK(proto_msg_tx_begin(&tx, "HELLO", 5));
    id = tx.msg_id;

    CHECK(!proto_msg_tx_ack(&tx, (uint16_t)(id + 1), 5));
    CHECK(tx.resume_pos == 0);

    CHECK(!proto_msg_tx_ack(&tx, id, 6));
    CHECK(tx.resume_pos == 6);

    /* An ACK for every symbol still does not complete before the final. */
    CHECK(!proto_msg_tx_ack(&tx, id, 15));
    CHECK(tx.phase == PROTO_TX_SENDING);
    CHECK(tx.resume_pos == 15);

    proto_msg_tx_final_sent(&tx, 1000, 500);
    CHECK(tx.phase == PROTO_TX_WAITING);
    CHECK(!proto_msg_tx_timed_out(&tx, 1499));
    CHECK(proto_msg_tx_timed_out(&tx, 1500));

    CHECK(proto_msg_tx_retry(&tx));
    CHECK(tx.phase == PROTO_TX_SENDING);
    CHECK(tx.pos == 15);
    CHECK(tx.attempt == 2);
    CHECK(tx.retransmits == 1);

    /* Only the final needs to be resent now. */
    proto_msg_tx_final_sent(&tx, 2000, 500);
    CHECK(proto_msg_tx_ack(&tx, id, 15));
    CHECK(tx.phase == PROTO_TX_IDLE);
    CHECK(tx.delivered == 1);
    CHECK(!proto_msg_tx_ack(&tx, id, 15));
}

static void test_tx_bounded_failure(void) {
    proto_msg_tx_t tx;
    int retries = 0;

    proto_msg_tx_init(&tx, ALPHABET, ALPHABET_LEN);
    CHECK(proto_msg_tx_begin(&tx, "SOS", 3));
    proto_msg_tx_final_sent(&tx, 0, 100);

    while (proto_msg_tx_retry(&tx)) {
        retries++;
        CHECK(retries < 100);
        proto_msg_tx_final_sent(&tx, 0, 100);
    }

    CHECK(tx.phase == PROTO_TX_FAILED);
    CHECK(tx.failed == 1);
    CHECK(tx.attempt == PROTO_MSG_MAX_ATTEMPTS);
    CHECK(retries == (int)PROTO_MSG_MAX_ATTEMPTS - 1);

    /* A failed message can be replaced by a new one. */
    CHECK(proto_msg_tx_begin(&tx, "OK", 2));
    CHECK(tx.phase == PROTO_TX_SENDING);
    CHECK(tx.attempt == 1);
    CHECK(tx.pos == 0);
}

int main(void) {
    test_rx_in_order_and_duplicate_final();
    test_rx_gap_and_resume();
    test_rx_whole_message_retry();
    test_rx_new_id_preempts_and_bounds();
    test_rx_bad_symbols();
    test_tx_symbols();
    test_tx_partial_ack_and_retry();
    test_tx_bounded_failure();

    if (failures) {
        printf("msg: %d failure(s)\n", failures);
        return 1;
    }
    printf("msg: PASS\n");
    return 0;
}
