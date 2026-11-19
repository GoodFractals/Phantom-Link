#include "proto_msg.h"

#include <string.h>

void proto_msg_rx_init(proto_msg_rx_t *rx, const char *alphabet,
                       uint8_t alphabet_len) {
    if (!rx) return;

    memset(rx, 0, sizeof *rx);
    rx->alphabet = alphabet;
    rx->alphabet_len = alphabet_len;
}

void proto_msg_rx_reset(proto_msg_rx_t *rx) {
    if (!rx) return;

    rx->completed_id = 0;
    rx->have_completed = false;
    rx->active_id = 0;
    rx->active = false;
    rx->symbol_count = 0;
    rx->total_symbols = 0;
    rx->digit_acc = 0;
    rx->digit_count = 0;
    rx->char_count = 0;
    rx->text[0] = '\0';
}

static void rx_begin(proto_msg_rx_t *rx, uint16_t msg_id) {
    rx->active = true;
    rx->active_id = msg_id;
    rx->symbol_count = 0;
    rx->digit_acc = 0;
    rx->digit_count = 0;
    rx->char_count = 0;
    rx->text[0] = '\0';
}

proto_msg_event_t proto_msg_rx_frame(proto_msg_rx_t *rx, uint16_t msg_id,
                                     uint8_t pos, uint8_t symbol, bool final) {
    if (!rx) return PROTO_MSG_EV_BAD;

    /* A retried message that was already delivered only needs an ACK. */
    if (rx->have_completed && msg_id == rx->completed_id) {
        if (final && pos == rx->total_symbols) {
            rx->duplicates++;
            return PROTO_MSG_EV_DUPLICATE;
        }
        return PROTO_MSG_EV_IGNORED;
    }

    /* A different message ID preempts an unfinished message. */
    if (rx->active && msg_id != rx->active_id) {
        rx->dropped++;
        rx->active = false;
    }

    /* Without a START flag the first symbol at position 0 is the start. A
     * later position with no active message means the beginning was lost. */
    if (!rx->active) {
        if (pos != 0) {
            rx->dropped++;
            return PROTO_MSG_EV_IGNORED;
        }
        rx_begin(rx, msg_id);
    }

    if (final) {
        if (pos != rx->symbol_count || rx->digit_count != 0 ||
            rx->char_count == 0) {
            rx->dropped++;
            return PROTO_MSG_EV_IGNORED;
        }

        rx->total_symbols = rx->symbol_count;
        rx->have_completed = true;
        rx->completed_id = rx->active_id;
        rx->active = false;
        rx->completed++;
        return PROTO_MSG_EV_COMPLETE;
    }

    /* Retries resend the whole message or the suffix from the first hole.
     * Positions below the prefix are duplicates; positions above it wait for
     * the hole to arrive. */
    if (pos < rx->symbol_count) {
        rx->duplicates++;
        return PROTO_MSG_EV_IGNORED;
    }
    if (pos > rx->symbol_count) {
        rx->dropped++;
        return PROTO_MSG_EV_IGNORED;
    }
    if (symbol >= 6 || rx->char_count >= PROTO_MSG_MAX_CHARS) {
        rx->dropped++;
        return PROTO_MSG_EV_BAD;
    }

    rx->symbol_count++;
    rx->digit_acc = (uint8_t)(rx->digit_acc * 6u + symbol);
    if (++rx->digit_count < 3) return PROTO_MSG_EV_SYMBOL;

    if (rx->digit_acc >= rx->alphabet_len) {
        rx->dropped++;
        rx->active = false;
        rx->digit_acc = 0;
        rx->digit_count = 0;
        return PROTO_MSG_EV_BAD;
    }

    rx->text[rx->char_count++] = rx->alphabet[rx->digit_acc];
    rx->text[rx->char_count] = '\0';
    rx->digit_acc = 0;
    rx->digit_count = 0;
    return PROTO_MSG_EV_SYMBOL;
}

uint8_t proto_msg_rx_next_pos(const proto_msg_rx_t *rx, uint16_t msg_id) {
    if (!rx) return 0;
    if (rx->have_completed && msg_id == rx->completed_id) {
        return rx->total_symbols;
    }
    if (rx->active && msg_id == rx->active_id) {
        return rx->symbol_count;
    }
    return 0;
}

const char *proto_msg_rx_text(const proto_msg_rx_t *rx) {
    return rx ? rx->text : "";
}

uint8_t proto_msg_rx_len(const proto_msg_rx_t *rx) {
    return rx ? rx->char_count : 0;
}

void proto_msg_tx_init(proto_msg_tx_t *tx, const char *alphabet,
                       uint8_t alphabet_len) {
    if (!tx) return;

    memset(tx, 0, sizeof *tx);
    tx->alphabet = alphabet;
    tx->alphabet_len = alphabet_len;
    tx->next_msg_id = 1;
    tx->phase = PROTO_TX_IDLE;
}

bool proto_msg_tx_begin(proto_msg_tx_t *tx, const char *text, uint8_t len) {
    if (!tx || !text || len == 0) return false;
    if (tx->phase == PROTO_TX_SENDING || tx->phase == PROTO_TX_WAITING) {
        return false;
    }
    if (len > PROTO_MSG_MAX_CHARS) len = PROTO_MSG_MAX_CHARS;

    memcpy(tx->text, text, len);
    tx->text[len] = '\0';
    tx->len = len;
    tx->total_symbols = (uint8_t)(len * 3u);
    tx->pos = 0;
    tx->resume_pos = 0;
    tx->waiting = false;
    tx->attempt = 1;
    tx->deadline_ms = 0;
    tx->phase = PROTO_TX_SENDING;

    tx->msg_id = tx->next_msg_id++;
    if (tx->next_msg_id == 0) tx->next_msg_id = 1;
    return true;
}

uint8_t proto_msg_tx_symbol(const proto_msg_tx_t *tx) {
    if (!tx || tx->pos >= tx->total_symbols) return 0;

    char c = tx->text[tx->pos / 3u];
    const char *p = strchr(tx->alphabet, c);
    int code = p ? (int)(p - tx->alphabet) : 0;

    unsigned divisor = (tx->pos % 3u == 0) ? 36u : (tx->pos % 3u == 1) ? 6u : 1u;
    return (uint8_t)(((unsigned)code / divisor) % 6u);
}

void proto_msg_tx_final_sent(proto_msg_tx_t *tx, uint32_t now,
                             uint32_t timeout_ms) {
    if (!tx) return;
    tx->waiting = true;
    tx->phase = PROTO_TX_WAITING;
    tx->deadline_ms = now + timeout_ms;
}

bool proto_msg_tx_timed_out(const proto_msg_tx_t *tx, uint32_t now) {
    if (!tx || tx->phase != PROTO_TX_WAITING) return false;
    return (int32_t)(now - tx->deadline_ms) >= 0;
}

bool proto_msg_tx_retry(proto_msg_tx_t *tx) {
    if (!tx || tx->phase != PROTO_TX_WAITING) return false;

    if (tx->attempt >= PROTO_MSG_MAX_ATTEMPTS) {
        tx->waiting = false;
        tx->phase = PROTO_TX_FAILED;
        tx->failed++;
        return false;
    }

    tx->attempt++;
    tx->retransmits++;
    tx->waiting = false;
    tx->pos = tx->resume_pos;
    tx->phase = PROTO_TX_SENDING;
    return true;
}

bool proto_msg_tx_ack(proto_msg_tx_t *tx, uint16_t msg_id, uint8_t next_pos) {
    if (!tx) return false;
    if (tx->phase != PROTO_TX_SENDING && tx->phase != PROTO_TX_WAITING) {
        return false;
    }
    if (msg_id != tx->msg_id) return false;

    if (next_pos > tx->total_symbols) next_pos = tx->total_symbols;
    if (next_pos > tx->resume_pos) tx->resume_pos = next_pos;

    if (tx->phase == PROTO_TX_WAITING && next_pos >= tx->total_symbols) {
        tx->waiting = false;
        tx->phase = PROTO_TX_IDLE;
        tx->delivered++;
        return true;
    }
    return false;
}

void proto_msg_tx_abort(proto_msg_tx_t *tx) {
    if (!tx) return;
    tx->waiting = false;
    tx->phase = PROTO_TX_IDLE;
}

const char *proto_msg_event_str(proto_msg_event_t event) {
    switch (event) {
    case PROTO_MSG_EV_IGNORED: return "ignored";
    case PROTO_MSG_EV_SYMBOL: return "symbol";
    case PROTO_MSG_EV_COMPLETE: return "complete";
    case PROTO_MSG_EV_DUPLICATE: return "duplicate";
    case PROTO_MSG_EV_BAD: return "bad";
    default: return "unknown";
    }
}
