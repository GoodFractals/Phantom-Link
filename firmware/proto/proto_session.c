#include "proto_session.h"

void proto_session_init(proto_session_t *session, uint32_t session_id) {
    if (!session) return;

    session->session_id = session_id;
    session->tx_seq = 0;
    session->rx_max = 0;
    session->rx_bitmap = 0;
    session->rx_started = false;
    session->accepted = 0;
    session->duplicates = 0;
    session->stale = 0;
    session->wrong_session = 0;
}

uint32_t proto_session_next_seq(proto_session_t *session) {
    if (!session) return 0;
    return session->tx_seq++;
}

proto_rx_result_t proto_session_accept(proto_session_t *session,
                                       uint32_t session_id, uint32_t seq) {
    uint32_t delta;
    uint32_t mask;

    if (!session) return PROTO_RX_WRONG_SESSION;

    if (session_id != session->session_id) {
        session->wrong_session++;
        return PROTO_RX_WRONG_SESSION;
    }

    if (!session->rx_started) {
        session->rx_started = true;
        session->rx_max = seq;
        session->rx_bitmap = 1u;
        session->accepted++;
        return PROTO_RX_ACCEPTED;
    }

    if (seq > session->rx_max) {
        delta = seq - session->rx_max;
        if (delta >= PROTO_REPLAY_WINDOW_BITS) {
            session->rx_bitmap = 0;
        } else {
            session->rx_bitmap <<= delta;
        }
        session->rx_bitmap |= 1u;
        session->rx_max = seq;
        session->accepted++;
        return PROTO_RX_ACCEPTED;
    }

    delta = session->rx_max - seq;
    if (delta >= PROTO_REPLAY_WINDOW_BITS) {
        session->stale++;
        return PROTO_RX_STALE;
    }

    mask = 1u << delta;
    if (session->rx_bitmap & mask) {
        session->duplicates++;
        return PROTO_RX_DUPLICATE;
    }

    session->rx_bitmap |= mask;
    session->accepted++;
    return PROTO_RX_ACCEPTED;
}

bool proto_session_active(const proto_session_t *session) {
    return session && session->session_id != 0;
}

const char *proto_rx_result_str(proto_rx_result_t result) {
    switch (result) {
    case PROTO_RX_ACCEPTED: return "accepted";
    case PROTO_RX_DUPLICATE: return "duplicate";
    case PROTO_RX_STALE: return "stale";
    case PROTO_RX_WRONG_SESSION: return "wrong-session";
    default: return "unknown";
    }
}
