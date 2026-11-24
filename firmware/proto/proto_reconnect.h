#ifndef PHANTOM_PROTO_RECONNECT_H
#define PHANTOM_PROTO_RECONNECT_H

/*
 * Host-testable reconnect state machine for the Bonnie & Clyde link.
 *
 * This module has no Pico SDK dependency and performs no I/O. main.c supplies
 * the monotonic time in milliseconds and the observed link state, then acts on
 * the returned action (start a handshake, retry the association, or tear down
 * the old session). It replaces the "reboot the STA after 15 s" workaround:
 * there is no reboot path here, only bounded retries and a deliberate
 * RECONNECTING state the HUD can render.
 *
 * Retry backoff starts at 500 ms, doubles on each attempt, and is capped at
 * 5000 ms. The delay for the action just taken is returned in every result as
 * backoff_ms, so the caller can log a bounded, visible line such as:
 *   printf("RECONNECTING attempt=%u backoff=%lums state=%s\n", ...);
 *
 * Sessions carry a generation (epoch). The caller supplies a strictly larger
 * epoch when a peer restarts and adopts a new session; frames or sessions from
 * an older epoch are rejected and invalidate the active session, so a stale
 * session can never coexist with its replacement. Zero means "no session",
 * matching proto_session's sentinel.
 */

#include <stdbool.h>
#include <stdint.h>

#define RECONNECT_BACKOFF_MIN_MS 500u
#define RECONNECT_BACKOFF_MAX_MS 5000u

typedef enum {
    RECONNECT_STATE_DOWN = 0,
    RECONNECT_STATE_CONNECTING,
    RECONNECT_STATE_RECONNECTING,
    RECONNECT_STATE_UP
} reconnect_state_t;

typedef enum {
    RECONNECT_ACTION_NONE = 0,
    RECONNECT_ACTION_START,
    RECONNECT_ACTION_RETRY,
    RECONNECT_ACTION_REPLACE,
    RECONNECT_ACTION_LINK_DOWN,
    RECONNECT_ACTION_LINK_UP
} reconnect_action_t;

typedef struct {
    reconnect_state_t state;
    uint32_t epoch;
    uint32_t backoff_ms;      /* current bounded retry delay */
    uint32_t attempts;        /* total connect/retry attempts */
    uint32_t replacements;    /* stale sessions replaced */
    uint32_t stale_rejected;  /* old-epoch observations rejected */
    uint32_t last_attempt_ms;
    uint32_t next_attempt_ms;
    bool session_active;
} reconnect_t;

typedef struct {
    reconnect_state_t state;
    reconnect_action_t action;
    uint32_t epoch;
    uint32_t backoff_ms;   /* delay before the next retry (0 when idle) */
    bool session_replaced; /* an older session was dropped this call */
    bool stale_rejected;   /* an old/equal epoch was refused this call */
    bool link_up;          /* this call moved the link to UP */
} reconnect_result_t;

/* Starts with no link, no session, and no scheduled retry. */
void reconnect_init(reconnect_t *rc);

/* Advances the state machine. link_observed means the Wi-Fi/UDP peer is
 * reachable; it does not by itself create a session. Time is a monotonic
 * uint32_t millisecond counter and may wrap. */
reconnect_result_t reconnect_tick(reconnect_t *rc, uint32_t now,
                                  bool link_observed);

/* Adopts a new session generation. new_epoch must be greater than the current
 * one; equal, lower, or zero epochs are refused. A successful adoption drops
 * any older session and moves to UP when link_observed is true (action
 * LINK_UP). If the link is down, the action is REPLACE when a previous epoch
 * was superseded, otherwise NONE. */
reconnect_result_t reconnect_adopt_epoch(reconnect_t *rc, uint32_t now,
                                         uint32_t new_epoch,
                                         bool link_observed);

/* Checks a session generation seen on an incoming frame. Returns a result
 * with stale_rejected set when observed_epoch is older than the current one;
 * that also invalidates the session and leaves UP for RECONNECTING. */
reconnect_result_t reconnect_accept_epoch(reconnect_t *rc, uint32_t now,
                                          uint32_t observed_epoch);

/* Stable strings for logs and UI-adjacent diagnostics. */
const char *reconnect_state_name(reconnect_state_t state);
const char *reconnect_action_name(reconnect_action_t action);

#endif
