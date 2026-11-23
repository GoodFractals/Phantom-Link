#include <stdio.h>
#include <string.h>

#include "proto_msg.h"
#include "proto_session.h"

/*
 * Two endpoints, a lost-datagram channel, and the real message state machines.
 * The frames here are logical (type, message ID, position, symbol); the byte
 * codec is covered by test_frame and test_wire.
 */

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
#define SESSION_ID 0xA1B2C3D4u

typedef enum { SIM_DATA = 1, SIM_ACK = 2 } sim_type_t;

typedef struct {
    sim_type_t type;
    uint16_t msg_id;
    uint8_t pos;
    uint8_t symbol;
    bool final;
    bool ack_req;
    bool final_ack;
    uint32_t seq;
} sim_frame_t;

typedef struct {
    uint32_t rng;
    int loss_percent;
    bool drop_first_final_ack;
    int sent;
    int dropped;
} channel_t;

typedef struct {
    proto_session_t session;
    proto_msg_rx_t rx;
    proto_msg_tx_t tx;
    uint32_t next_ms;
    uint32_t interval_ms;
    uint32_t ack_timeout_ms;
} endpoint_t;

static uint32_t rng_next(channel_t *ch) {
    uint32_t x = ch->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    ch->rng = x ? x : 0x9E3779B9u;
    return ch->rng;
}

static void endpoint_init(endpoint_t *e, uint32_t interval_ms,
                          uint32_t ack_timeout_ms) {
    proto_session_init(&e->session, SESSION_ID);
    proto_msg_rx_init(&e->rx, ALPHABET, ALPHABET_LEN);
    proto_msg_tx_init(&e->tx, ALPHABET, ALPHABET_LEN);
    e->next_ms = 0;
    e->interval_ms = interval_ms;
    e->ack_timeout_ms = ack_timeout_ms;
}

static void channel_send(channel_t *ch, const sim_frame_t *f, endpoint_t *from,
                         endpoint_t *to, uint32_t now);

static void deliver(channel_t *ch, const sim_frame_t *f, endpoint_t *from,
                    endpoint_t *to, uint32_t now) {
    proto_rx_result_t verdict =
        proto_session_accept(&to->session, SESSION_ID, f->seq);
    if (verdict != PROTO_RX_ACCEPTED) {
        CHECK(verdict == PROTO_RX_DUPLICATE || verdict == PROTO_RX_STALE);
        return;
    }

    if (f->type == SIM_DATA) {
        (void)proto_msg_rx_frame(&to->rx, f->msg_id, f->pos, f->symbol,
                                 f->final);
        if (f->ack_req || f->final) {
            sim_frame_t ack;
            memset(&ack, 0, sizeof ack);
            ack.type = SIM_ACK;
            ack.msg_id = f->msg_id;
            ack.pos = proto_msg_rx_next_pos(&to->rx, f->msg_id);
            ack.final_ack = f->final;
            ack.seq = proto_session_next_seq(&to->session);
            channel_send(ch, &ack, to, from, now);
        }
    } else {
        (void)proto_msg_tx_ack(&to->tx, f->msg_id, f->pos);
    }
}

static void channel_send(channel_t *ch, const sim_frame_t *f, endpoint_t *from,
                         endpoint_t *to, uint32_t now) {
    ch->sent++;

    if (ch->loss_percent >= 100 ||
        (ch->loss_percent > 0 &&
         (rng_next(ch) % 100u) < (uint32_t)ch->loss_percent)) {
        ch->dropped++;
        return;
    }

    /* Drop exactly one final ACK to exercise the lost-ACK path. */
    if (f->type == SIM_ACK && f->final_ack && ch->drop_first_final_ack) {
        ch->drop_first_final_ack = false;
        ch->dropped++;
        return;
    }

    deliver(ch, f, from, to, now);
}

static void endpoint_tick(endpoint_t *e, endpoint_t *peer, channel_t *ch,
                          uint32_t now) {
    if (e->tx.phase == PROTO_TX_SENDING &&
        (int32_t)(now - e->next_ms) >= 0) {
        sim_frame_t f;
        memset(&f, 0, sizeof f);
        f.type = SIM_DATA;
        f.msg_id = e->tx.msg_id;
        f.seq = proto_session_next_seq(&e->session);

        if (e->tx.pos < e->tx.total_symbols) {
            f.pos = e->tx.pos;
            f.symbol = proto_msg_tx_symbol(&e->tx);
            /* ACK every 8th DATA frame, counted from 1. */
            f.ack_req = ((uint8_t)(e->tx.pos + 1u) % 8u) == 0u;
            channel_send(ch, &f, e, peer, now);
            e->tx.pos++;
        } else {
            f.pos = e->tx.total_symbols;
            f.final = true;
            f.ack_req = true;
            /* Mark the wait first: the harness delivers the ACK synchronously
             * inside the send, unlike UDP where it arrives on a later poll. */
            proto_msg_tx_final_sent(&e->tx, now, e->ack_timeout_ms);
            channel_send(ch, &f, e, peer, now);
        }
        e->next_ms = now + e->interval_ms;
    }

    if (proto_msg_tx_timed_out(&e->tx, now)) {
        (void)proto_msg_tx_retry(&e->tx);
        e->next_ms = now;
    }
}

static bool run_link(endpoint_t *a, endpoint_t *b, channel_t *ch,
                     uint32_t max_ms, uint32_t *elapsed_ms) {
    uint32_t now = 0;

    while (now < max_ms) {
        endpoint_tick(a, b, ch, now);
        endpoint_tick(b, a, ch, now);

        bool a_done = (a->tx.phase == PROTO_TX_IDLE ||
                       a->tx.phase == PROTO_TX_FAILED);
        bool b_done = (b->tx.phase == PROTO_TX_IDLE ||
                       b->tx.phase == PROTO_TX_FAILED);
        if (a_done && b_done) {
            *elapsed_ms = now;
            return true;
        }
        now++;
    }

    *elapsed_ms = now;
    return false;
}

static void start_pair(endpoint_t *a, endpoint_t *b, const char *a_text,
                       const char *b_text) {
    CHECK(proto_msg_tx_begin(&a->tx, a_text, (uint8_t)strlen(a_text)));
    CHECK(proto_msg_tx_begin(&b->tx, b_text, (uint8_t)strlen(b_text)));
}

static void test_ten_percent_loss_both_directions(void) {
    endpoint_t a;
    endpoint_t b;
    channel_t ch;
    uint32_t elapsed = 0;
    const char *a_text = "ABCDEFGHIJKLMNOPQRSTU"; /* 21 characters */
    const char *b_text = "ZYXWVUTSRQPONMLKJIHGF";

    endpoint_init(&a, 5, 60);
    endpoint_init(&b, 5, 60);
    memset(&ch, 0, sizeof ch);
    ch.rng = 0x12345678u;
    ch.loss_percent = 10;

    start_pair(&a, &b, a_text, b_text);
    CHECK(run_link(&a, &b, &ch, 60000, &elapsed));

    CHECK(a.tx.phase == PROTO_TX_IDLE);
    CHECK(b.tx.phase == PROTO_TX_IDLE);
    CHECK(a.tx.delivered == 1);
    CHECK(b.tx.delivered == 1);

    CHECK(a.rx.completed == 1);
    CHECK(b.rx.completed == 1);
    CHECK(strcmp(proto_msg_rx_text(&a.rx), b_text) == 0);
    CHECK(strcmp(proto_msg_rx_text(&b.rx), a_text) == 0);

    /* 10% loss on both directions must have caused retries but no duplicate
     * delivery and no failure. */
    CHECK(a.tx.retransmits + b.tx.retransmits > 0);
    CHECK(a.tx.failed == 0);
    CHECK(b.tx.failed == 0);
    CHECK(ch.dropped > 0);

    printf("reliable: 10%% loss, %d datagrams, %d dropped, %u ms, "
           "retries %u/%u\n",
           ch.sent, ch.dropped, (unsigned)elapsed,
           (unsigned)a.tx.retransmits, (unsigned)b.tx.retransmits);
}

static void test_lost_ack_does_not_duplicate(void) {
    endpoint_t a;
    endpoint_t b;
    channel_t ch;
    uint32_t elapsed = 0;
    const char *text = "HELLO WORLD";

    endpoint_init(&a, 5, 60);
    endpoint_init(&b, 5, 60);
    memset(&ch, 0, sizeof ch);
    ch.rng = 0xDEADBEEFu;
    ch.loss_percent = 0;
    ch.drop_first_final_ack = true;

    CHECK(proto_msg_tx_begin(&a.tx, text, (uint8_t)strlen(text)));
    CHECK(run_link(&a, &b, &ch, 30000, &elapsed));

    CHECK(a.tx.phase == PROTO_TX_IDLE);
    CHECK(a.tx.delivered == 1);
    CHECK(a.tx.retransmits >= 1);
    CHECK(b.rx.completed == 1);
    CHECK(strcmp(proto_msg_rx_text(&b.rx), text) == 0);
    CHECK(b.rx.duplicates >= 1);
}

static void test_all_lost_stops_and_reports_failure(void) {
    endpoint_t a;
    endpoint_t b;
    channel_t ch;
    uint32_t elapsed = 0;

    endpoint_init(&a, 2, 20);
    endpoint_init(&b, 2, 20);
    memset(&ch, 0, sizeof ch);
    ch.loss_percent = 100;

    CHECK(proto_msg_tx_begin(&a.tx, "SOS", 3));
    CHECK(run_link(&a, &b, &ch, 60000, &elapsed));

    CHECK(a.tx.phase == PROTO_TX_FAILED);
    CHECK(a.tx.failed == 1);
    CHECK(a.tx.attempt == PROTO_MSG_MAX_ATTEMPTS);
    CHECK(b.rx.completed == 0);
}

static void test_repeated_messages_stay_bounded(void) {
    endpoint_t a;
    endpoint_t b;
    char text[PROTO_MSG_MAX_CHARS + 1];
    int i;

    endpoint_init(&a, 1, 15);
    endpoint_init(&b, 1, 15);

    for (i = 0; i < 40; i++) {
        channel_t ch;
        uint32_t elapsed = 0;

        memset(&ch, 0, sizeof ch);
        ch.rng = 0x5000u + (uint32_t)i * 7919u;
        ch.loss_percent = 10;

        memset(text, 'A' + (i % 26), PROTO_MSG_MAX_CHARS);
        text[PROTO_MSG_MAX_CHARS] = '\0';

        CHECK(proto_msg_tx_begin(&a.tx, text, PROTO_MSG_MAX_CHARS));
        CHECK(run_link(&a, &b, &ch, 60000, &elapsed));
        CHECK(a.tx.phase == PROTO_TX_IDLE);
        CHECK(b.rx.completed == (uint32_t)(i + 1));
        CHECK(strcmp(proto_msg_rx_text(&b.rx), text) == 0);
        CHECK(proto_msg_rx_len(&b.rx) <= PROTO_MSG_MAX_CHARS);
    }

    CHECK(b.rx.duplicates > 0);
    CHECK(a.tx.retransmits > 0);
}

int main(void) {
    test_ten_percent_loss_both_directions();
    test_lost_ack_does_not_duplicate();
    test_all_lost_stops_and_reports_failure();
    test_repeated_messages_stay_bounded();

    if (failures) {
        printf("reliable: %d failure(s)\n", failures);
        return 1;
    }
    printf("reliable: PASS\n");
    return 0;
}
