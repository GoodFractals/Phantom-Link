#ifndef PHANTOM_PROTO_SESSION_H
#define PHANTOM_PROTO_SESSION_H

/*
 * Per-session transmit sequencing and the receive replay window.
 *
 * The module has no Pico SDK dependency. main.c supplies the session ID from
 * the hardware RNG and calls proto_session_next_seq for every outgoing frame.
 * Every incoming frame passes proto_session_accept before the application sees
 * it.
 *
 * The window is the 32-bit sliding bitmap from the gameplan: bit 0 is the
 * highest accepted sequence, bit n is n below it. A sequence at or below
 * max-32 is stale. A sequence whose bit is already set is a duplicate. Both
 * are counted and never reach the app.
 */

#include <stdbool.h>
#include <stdint.h>

#define PROTO_REPLAY_WINDOW_BITS 32u

typedef enum {
    PROTO_RX_ACCEPTED = 0,
    PROTO_RX_DUPLICATE,
    PROTO_RX_STALE,
    PROTO_RX_WRONG_SESSION
} proto_rx_result_t;

typedef struct {
    uint32_t session_id;
    uint32_t tx_seq;
    uint32_t rx_max;
    uint32_t rx_bitmap;
    bool rx_started;

    /* Counters are part of the state so a reboot and a reset are measurable. */
    uint32_t accepted;
    uint32_t duplicates;
    uint32_t stale;
    uint32_t wrong_session;
} proto_session_t;

/* Resets transmit sequence, receive window, and counters for a new session. */
void proto_session_init(proto_session_t *session, uint32_t session_id);

/* Returns the next transmit sequence and advances it. Starts at 0. */
uint32_t proto_session_next_seq(proto_session_t *session);

/* Checks the session ID and replay window. Never changes state on rejection. */
proto_rx_result_t proto_session_accept(proto_session_t *session,
                                       uint32_t session_id, uint32_t seq);

/* True when the session has a non-zero ID and has not been reset. */
bool proto_session_active(const proto_session_t *session);

const char *proto_rx_result_str(proto_rx_result_t result);

#endif
