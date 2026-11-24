#include "proto_reconnect.h"

/* True when now has reached deadline, including across a uint32_t wrap. */
static bool time_reached(uint32_t now, uint32_t deadline) {
    return now >= deadline || (deadline - now) > (UINT32_MAX / 2u);
}

static reconnect_result_t result_for(const reconnect_t *rc) {
    reconnect_result_t result;

    result.state = rc->state;
    result.action = RECONNECT_ACTION_NONE;
    result.epoch = rc->epoch;
    result.backoff_ms = rc->backoff_ms;
    result.session_replaced = false;
    result.stale_rejected = false;
    result.link_up = false;
    return result;
}

/* Grows the bounded backoff and schedules the next attempt at now + backoff. */
static void schedule_attempt(reconnect_t *rc, uint32_t now) {
    if (rc->backoff_ms == 0) {
        rc->backoff_ms = RECONNECT_BACKOFF_MIN_MS;
    } else if (rc->backoff_ms >= RECONNECT_BACKOFF_MAX_MS) {
        rc->backoff_ms = RECONNECT_BACKOFF_MAX_MS;
    } else if (rc->backoff_ms > RECONNECT_BACKOFF_MAX_MS / 2u) {
        rc->backoff_ms = RECONNECT_BACKOFF_MAX_MS;
    } else {
        rc->backoff_ms *= 2u;
    }

    rc->attempts++;
    rc->last_attempt_ms = now;
    rc->next_attempt_ms = now + rc->backoff_ms;
}

void reconnect_init(reconnect_t *rc) {
    rc->state = RECONNECT_STATE_DOWN;
    rc->epoch = 0;
    rc->backoff_ms = 0;
    rc->attempts = 0;
    rc->replacements = 0;
    rc->stale_rejected = 0;
    rc->last_attempt_ms = 0;
    rc->next_attempt_ms = 0;
    rc->session_active = false;
}

reconnect_result_t reconnect_tick(reconnect_t *rc, uint32_t now,
                                  bool link_observed) {
    reconnect_result_t result = result_for(rc);

    switch (rc->state) {
    case RECONNECT_STATE_UP:
        if (!link_observed) {
            rc->state = RECONNECT_STATE_RECONNECTING;
            rc->session_active = false;
            rc->backoff_ms = RECONNECT_BACKOFF_MIN_MS;
            rc->last_attempt_ms = now;
            rc->next_attempt_ms = now + RECONNECT_BACKOFF_MIN_MS;
            result.state = rc->state;
            result.action = RECONNECT_ACTION_LINK_DOWN;
            result.backoff_ms = rc->backoff_ms;
        }
        break;

    case RECONNECT_STATE_DOWN:
        if (link_observed) {
            rc->state = RECONNECT_STATE_CONNECTING;
            schedule_attempt(rc, now);
            result.state = rc->state;
            result.action = RECONNECT_ACTION_START;
            result.backoff_ms = rc->backoff_ms;
        }
        break;

    case RECONNECT_STATE_CONNECTING:
        if (!link_observed) {
            rc->state = RECONNECT_STATE_DOWN;
            rc->backoff_ms = 0;
            rc->next_attempt_ms = 0;
            result.state = rc->state;
            result.action = RECONNECT_ACTION_LINK_DOWN;
            result.backoff_ms = 0;
        } else if (time_reached(now, rc->next_attempt_ms)) {
            schedule_attempt(rc, now);
            result.action = RECONNECT_ACTION_START;
            result.backoff_ms = rc->backoff_ms;
        }
        break;

    case RECONNECT_STATE_RECONNECTING:
        if (time_reached(now, rc->next_attempt_ms)) {
            schedule_attempt(rc, now);
            result.action = RECONNECT_ACTION_RETRY;
            result.backoff_ms = rc->backoff_ms;
        }
        break;

    default:
        break;
    }

    return result;
}

reconnect_result_t reconnect_adopt_epoch(reconnect_t *rc, uint32_t now,
                                         uint32_t new_epoch,
                                         bool link_observed) {
    reconnect_result_t result = result_for(rc);

    if (new_epoch == 0) {
        return result;
    }
    if (rc->epoch != 0 && new_epoch <= rc->epoch) {
        rc->stale_rejected++;
        result.stale_rejected = true;
        return result;
    }

    if (rc->epoch != 0) {
        rc->replacements++;
        result.session_replaced = true;
    }

    rc->epoch = new_epoch;
    rc->session_active = true;
    rc->backoff_ms = 0;
    rc->last_attempt_ms = now;
    rc->next_attempt_ms = now;
    result.epoch = new_epoch;
    result.backoff_ms = 0;

    if (link_observed) {
        rc->state = RECONNECT_STATE_UP;
        result.action = RECONNECT_ACTION_LINK_UP;
        result.link_up = true;
    } else if (rc->state == RECONNECT_STATE_DOWN) {
        rc->state = RECONNECT_STATE_CONNECTING;
    } else if (rc->state == RECONNECT_STATE_UP) {
        rc->state = RECONNECT_STATE_RECONNECTING;
    }
    if (!link_observed && result.session_replaced) {
        result.action = RECONNECT_ACTION_REPLACE;
    }
    result.state = rc->state;
    return result;
}

reconnect_result_t reconnect_accept_epoch(reconnect_t *rc, uint32_t now,
                                          uint32_t observed_epoch) {
    reconnect_result_t result = result_for(rc);

    if (rc->epoch == 0 || observed_epoch == rc->epoch ||
        observed_epoch > rc->epoch) {
        return result;
    }

    /* Older than the current generation: the old session is no longer
     * admissible and must not survive alongside the replacement. */
    rc->stale_rejected++;
    rc->session_active = false;
    result.stale_rejected = true;

    if (rc->state == RECONNECT_STATE_UP) {
        rc->state = RECONNECT_STATE_RECONNECTING;
        rc->backoff_ms = RECONNECT_BACKOFF_MIN_MS;
        rc->last_attempt_ms = now;
        rc->next_attempt_ms = now + RECONNECT_BACKOFF_MIN_MS;
        result.state = rc->state;
        result.action = RECONNECT_ACTION_LINK_DOWN;
        result.backoff_ms = rc->backoff_ms;
    }
    return result;
}

const char *reconnect_state_name(reconnect_state_t state) {
    switch (state) {
    case RECONNECT_STATE_DOWN:
        return "DOWN";
    case RECONNECT_STATE_CONNECTING:
        return "CONNECTING";
    case RECONNECT_STATE_RECONNECTING:
        return "RECONNECTING";
    case RECONNECT_STATE_UP:
        return "UP";
    default:
        return "unknown";
    }
}

const char *reconnect_action_name(reconnect_action_t action) {
    switch (action) {
    case RECONNECT_ACTION_NONE:
        return "none";
    case RECONNECT_ACTION_START:
        return "start";
    case RECONNECT_ACTION_RETRY:
        return "retry";
    case RECONNECT_ACTION_REPLACE:
        return "replace";
    case RECONNECT_ACTION_LINK_DOWN:
        return "link-down";
    case RECONNECT_ACTION_LINK_UP:
        return "link-up";
    default:
        return "unknown";
    }
}
