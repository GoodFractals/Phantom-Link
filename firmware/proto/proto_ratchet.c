#include "proto_ratchet.h"

#include <string.h>

/* Serial-number comparison limited to the window: a candidate is newer only
 * for forward distances in [1, 2^31-1]. Larger distances mean "older" once
 * the uint32_t counter has wrapped. */
static bool id_is_newer(uint32_t candidate, uint32_t tip) {
    uint32_t forward = candidate - tip;
    return forward != 0u && forward < 0x80000000u;
}

static void derive_key(const uint8_t master[PC_MASTER_KEY_BYTES],
                       uint32_t msg_id,
                       uint8_t out[PC_DERIVED_KEY_BYTES]) {
    uint8_t salt[RATCHET_MSG_ID_BYTES];

    salt[0] = (uint8_t)msg_id;
    salt[1] = (uint8_t)(msg_id >> 8u);
    salt[2] = (uint8_t)(msg_id >> 16u);
    salt[3] = (uint8_t)(msg_id >> 24u);
    pc_kdf(master, salt, sizeof salt, RATCHET_LABEL, out);
}

/* Slot 0 is the tip, slot n is tip-n (mod 2^32). RATCHET_WINDOW means "not in
 * the window" and is kept out of every array access. */
static unsigned window_slot(uint32_t tip, uint32_t msg_id) {
    uint32_t back = tip - msg_id;
    return (back < (uint32_t)RATCHET_WINDOW) ? (unsigned)back : RATCHET_WINDOW;
}

/* Rebuilds the whole window ending at new_tip. Deriving all eight keys also
 * covers gaps: if the peer jumped ahead, lost IDs inside the new window are
 * still decryptable when they arrive late. */
static void fill_window(ratchet_t *st, uint32_t new_tip) {
    unsigned i;

    for (i = 0; i < RATCHET_WINDOW; i++) {
        uint32_t id = new_tip - (uint32_t)i;
        st->ids[i] = id;
        derive_key(st->master_key, id, st->keys[i]);
        st->used[i] = true;
    }
    st->tip = new_tip;
    st->has_tip = true;
}

void ratchet_init(ratchet_t *st, const uint8_t master[PC_MASTER_KEY_BYTES]) {
    if (st == NULL) {
        return;
    }
    memset(st, 0, sizeof *st);
    if (master != NULL) {
        memcpy(st->master_key, master, PC_MASTER_KEY_BYTES);
        st->ready = true;
    }
}

bool ratchet_advance(ratchet_t *st, uint32_t msg_id,
                     uint8_t out_key[PC_DERIVED_KEY_BYTES]) {
    unsigned slot;

    if (out_key == NULL) {
        return false;
    }
    if (st == NULL || !st->ready) {
        memset(out_key, 0, PC_DERIVED_KEY_BYTES);
        return false;
    }

    if (!st->has_tip || id_is_newer(msg_id, st->tip)) {
        fill_window(st, msg_id);
    }

    slot = window_slot(st->tip, msg_id);
    if (slot >= RATCHET_WINDOW || !st->used[slot]) {
        st->rollbacks_rejected++;
        memset(out_key, 0, PC_DERIVED_KEY_BYTES);
        return false;
    }

    memcpy(out_key, st->keys[slot], PC_DERIVED_KEY_BYTES);
    st->accepted++;
    return true;
}

bool ratchet_key_for(ratchet_t *st, uint32_t msg_id,
                     uint8_t out_key[PC_DERIVED_KEY_BYTES]) {
    unsigned slot;

    if (out_key == NULL) {
        return false;
    }
    if (st == NULL || !st->ready || !st->has_tip) {
        memset(out_key, 0, PC_DERIVED_KEY_BYTES);
        return false;
    }

    slot = window_slot(st->tip, msg_id);
    if (slot >= RATCHET_WINDOW || !st->used[slot]) {
        st->rollbacks_rejected++;
        memset(out_key, 0, PC_DERIVED_KEY_BYTES);
        return false;
    }

    memcpy(out_key, st->keys[slot], PC_DERIVED_KEY_BYTES);
    return true;
}
