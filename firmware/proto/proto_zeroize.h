#ifndef PHANTOM_PROTO_ZEROIZE_H
#define PHANTOM_PROTO_ZEROIZE_H

/*
 * Secret zeroization for Phantom Link.
 *
 * Host-testable and SDK-free: no Pico SDK headers, no I/O, no allocation.
 * Every wipe goes through volatile byte writes, so an optimizing compiler
 * cannot discard a store it considers dead (plain memset can be removed).
 *
 * zeroize() wipes one caller-owned buffer. zeroize_tracker_t records a bounded
 * set of (buffer, length) pairs so BYE and power-loss paths can wipe every
 * live secret with one call. The tracker stores pointers only; it never owns
 * or allocates the buffers it tracks.
 */

#include <stdbool.h>
#include <stddef.h>

/* Maximum number of secrets a tracker can hold at once. */
#define ZEROIZE_TRACKER_MAX 16u

typedef struct {
    void *buf;
    size_t len;
} zeroize_entry_t;

typedef struct {
    zeroize_entry_t entries[ZEROIZE_TRACKER_MAX];
    size_t count;
} zeroize_tracker_t;

/* Writes len zero bytes to buf through volatile stores. NULL or len == 0 is
 * a no-op, so callers can wipe optional buffers unconditionally. */
void zeroize(void *buf, size_t len);

/* Prepares an empty tracker and scrubs any stale entries from prior use.
 * Does not touch the buffers those stale entries pointed at. NULL is a no-op.
 * Callers must init a tracker before registering into it. */
void zeroize_tracker_init(zeroize_tracker_t *tracker);

/* Records a secret for later bulk wiping. Fails (returns false) for a NULL
 * tracker, a NULL buffer, a zero length, or a full tracker. */
bool zeroize_tracker_register(zeroize_tracker_t *tracker, void *buf,
                              size_t len);

/* Wipes every registered buffer, then empties the tracker. Idempotent: a
 * second call has nothing left to wipe. NULL is a no-op. */
void zeroize_tracker_clear_all(zeroize_tracker_t *tracker);

/* Number of currently registered secrets; 0 for a NULL tracker. */
size_t zeroize_tracker_count(const zeroize_tracker_t *tracker);

#endif
