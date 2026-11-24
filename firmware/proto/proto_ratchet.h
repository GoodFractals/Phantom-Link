#ifndef PHANTOM_PROTO_RATCHET_H
#define PHANTOM_PROTO_RATCHET_H

/* Host-only per-message key ratchet for the Bonnie & Clyde link.
 *
 * No Pico SDK headers and no I/O here: this module compiles into the host
 * tests on its own. Each message ID gets a distinct 32-byte traffic key from
 * pc_kdf(master, LE32(msg_id), label 0x04). Compromise of one derived key does
 * not reveal any other message key; the master itself is never exported.
 *
 * The caller supplies a direction-specific master (the session's tx or rx
 * key, already separated by the 0x01/0x02 KDF labels). This module adds the
 * per-message separation on top of that direction binding; it does not invent
 * a second direction label.
 *
 * An 8-slot window keeps the tip plus its seven predecessors, so a message
 * delivered out of order still finds its cached key. IDs use serial-number
 * (wrap-aware) arithmetic; an ID older than the window is a rollback and is
 * rejected. key_for() is a read-only lookup and never moves the tip; advance()
 * is the only operation that accepts a newer ID.
 *
 * State is in-memory only. It survives a reboot only if the session (and thus
 * its master key) survives: call ratchet_init() again with the same session
 * master after a reboot, never with a fresh handshake key.
 */

#include <stdbool.h>
#include <stdint.h>

#include "proto_crypto.h"

#define RATCHET_WINDOW 8u
#define RATCHET_LABEL 0x04u
#define RATCHET_MSG_ID_BYTES 4u

typedef struct {
    uint8_t master_key[PC_MASTER_KEY_BYTES];
    uint32_t tip;                                    /* newest accepted ID */
    uint32_t ids[RATCHET_WINDOW];                    /* slot 0 == tip */
    uint8_t keys[RATCHET_WINDOW][PC_DERIVED_KEY_BYTES];
    bool used[RATCHET_WINDOW];
    bool ready;                                      /* master was supplied */
    bool has_tip;                                    /* at least one advance */
    uint32_t accepted;                               /* successful lookups */
    uint32_t rollbacks_rejected;                     /* stale/outside window */
} ratchet_t;

/* Prepares an empty window. A NULL state is a no-op; a NULL master leaves the
 * state unusable (every later call fails safely). */
void ratchet_init(ratchet_t *st, const uint8_t master[PC_MASTER_KEY_BYTES]);

/* Accepts msg_id. A newer ID slides the window and yields its derived key;
 * an ID inside the current window yields the cached key without moving the
 * tip. A rollback outside the window is rejected. Always writes exactly
 * PC_DERIVED_KEY_BYTES to out_key (zero on rejection). Returns false for a
 * NULL state or out_key. */
bool ratchet_advance(ratchet_t *st, uint32_t msg_id,
                     uint8_t out_key[PC_DERIVED_KEY_BYTES]);

/* Read-only lookup of an ID already in the window. Never changes the tip.
 * A stale/future ID or a NULL argument is rejected and out_key is zeroed. */
bool ratchet_key_for(ratchet_t *st, uint32_t msg_id,
                     uint8_t out_key[PC_DERIVED_KEY_BYTES]);

#endif
