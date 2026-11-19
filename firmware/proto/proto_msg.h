#ifndef PHANTOM_PROTO_MSG_H
#define PHANTOM_PROTO_MSG_H

/*
 * End-to-end message assembly and retransmission.
 *
 * DATA payload layout:
 *   symbol frame: msg_id(2, big endian), pos(1), base-6 symbol(1)
 *   final frame:  msg_id(2, big endian), pos(1), flag FINAL; pos is the total
 *                 number of symbols in the message.
 *
 * ACK payload layout:
 *   msg_id(2, big endian), next_pos(1)
 *
 * next_pos is the first symbol position the receiver still needs. The sender
 * resumes there after a timeout, so every retry is idempotent: the receiver
 * ignores positions it already has and never inserts a character twice. When
 * no ACK arrives at all, the sender retransmits the whole message from 0.
 *
 * A message is at most 21 characters, 63 symbols. The message ID is 16 bits
 * and increases per message; it wraps after 65,535 messages, which cannot
 * collide with a retry because only one message is ever in flight.
 *
 * The module has no Pico SDK dependency. The caller supplies the 49-code
 * transport alphabet.
 */

#include <stdbool.h>
#include <stdint.h>

#define PROTO_MSG_MAX_CHARS 21u
#define PROTO_MSG_MAX_SYMBOLS (PROTO_MSG_MAX_CHARS * 3u)
#define PROTO_MSG_SYMBOL_PAYLOAD 4u
#define PROTO_MSG_FINAL_PAYLOAD 3u
#define PROTO_MSG_ACK_PAYLOAD 3u
#define PROTO_MSG_MAX_ATTEMPTS 20u

typedef enum {
    PROTO_MSG_EV_IGNORED = 0,
    PROTO_MSG_EV_SYMBOL,
    PROTO_MSG_EV_COMPLETE,
    PROTO_MSG_EV_DUPLICATE,
    PROTO_MSG_EV_BAD
} proto_msg_event_t;

typedef struct {
    const char *alphabet;
    uint8_t alphabet_len;

    uint16_t completed_id;
    bool have_completed;
    uint16_t active_id;
    bool active;
    uint8_t symbol_count;
    uint8_t total_symbols;
    uint8_t digit_acc;
    uint8_t digit_count;
    uint8_t char_count;
    char text[PROTO_MSG_MAX_CHARS + 1u];

    uint32_t completed;
    uint32_t duplicates;
    uint32_t dropped;
} proto_msg_rx_t;

typedef enum {
    PROTO_TX_IDLE = 0,
    PROTO_TX_SENDING,
    PROTO_TX_WAITING,
    PROTO_TX_FAILED
} proto_tx_phase_t;

typedef struct {
    const char *alphabet;
    uint8_t alphabet_len;

    char text[PROTO_MSG_MAX_CHARS + 1u];
    uint8_t len;
    uint8_t total_symbols;
    uint8_t pos;
    uint8_t resume_pos;
    uint16_t msg_id;
    uint16_t next_msg_id;
    uint8_t attempt;
    bool waiting;
    proto_tx_phase_t phase;
    uint32_t deadline_ms;

    uint32_t retransmits;
    uint32_t delivered;
    uint32_t failed;
} proto_msg_tx_t;

/* Receiver ------------------------------------------------------------------ */

void proto_msg_rx_init(proto_msg_rx_t *rx, const char *alphabet,
                       uint8_t alphabet_len);

/* Clears message state for a new session; lifetime counters are kept. */
void proto_msg_rx_reset(proto_msg_rx_t *rx);

/* Feeds one DATA frame. `final` means the payload carried the total position
 * and no symbol. Returns COMPLETE exactly once per delivered message. */
proto_msg_event_t proto_msg_rx_frame(proto_msg_rx_t *rx, uint16_t msg_id,
                                     uint8_t pos, uint8_t symbol, bool final);

/* Position the peer should send next: 0, the contiguous prefix, or the total
 * for a message already delivered. */
uint8_t proto_msg_rx_next_pos(const proto_msg_rx_t *rx, uint16_t msg_id);

const char *proto_msg_rx_text(const proto_msg_rx_t *rx);
uint8_t proto_msg_rx_len(const proto_msg_rx_t *rx);

/* Sender -------------------------------------------------------------------- */

void proto_msg_tx_init(proto_msg_tx_t *tx, const char *alphabet,
                       uint8_t alphabet_len);

/* Starts a new message. Returns false if one is already in flight or empty. */
bool proto_msg_tx_begin(proto_msg_tx_t *tx, const char *text, uint8_t len);

/* Base-6 digit at the current position, 0..5. */
uint8_t proto_msg_tx_symbol(const proto_msg_tx_t *tx);

/* Records that the FINAL frame was handed to the network. */
void proto_msg_tx_final_sent(proto_msg_tx_t *tx, uint32_t now,
                             uint32_t timeout_ms);

bool proto_msg_tx_timed_out(const proto_msg_tx_t *tx, uint32_t now);

/* Advances to the next attempt from the first unacknowledged position.
 * Returns false and enters FAILED after PROTO_MSG_MAX_ATTEMPTS. */
bool proto_msg_tx_retry(proto_msg_tx_t *tx);

/* Applies an ACK. Returns true when the FINAL frame is acknowledged. */
bool proto_msg_tx_ack(proto_msg_tx_t *tx, uint16_t msg_id, uint8_t next_pos);

void proto_msg_tx_abort(proto_msg_tx_t *tx);

const char *proto_msg_event_str(proto_msg_event_t event);

#endif
