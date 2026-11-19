#ifndef PHANTOM_PROTO_TXQ_H
#define PHANTOM_PROTO_TXQ_H

/*
 * Bounded transmit queue.
 *
 * A fixed-size ring buffer of outgoing DATA frames. The main loop drains at
 * most one item per tick, so the radio and the UI never stall behind a long
 * message. Enqueueing into a full queue fails and increments the overflow
 * counter instead of silently dropping a frame.
 *
 * The module has no Pico SDK dependency.
 */

#include <stdbool.h>
#include <stdint.h>

#define TXQ_DEPTH 64u
#define TXQ_PAYLOAD_MAX 4u

typedef struct {
    uint8_t flags;
    uint8_t payload[TXQ_PAYLOAD_MAX];
    uint8_t payload_len;
} txq_item_t;

typedef struct {
    txq_item_t items[TXQ_DEPTH];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    uint32_t enqueued;
    uint32_t sent;
    uint32_t dropped;
    uint32_t overflows;
} txq_t;

void txq_init(txq_t *q);
bool txq_enqueue(txq_t *q, const txq_item_t *item);
bool txq_peek(const txq_t *q, txq_item_t *out);
bool txq_dequeue(txq_t *q, txq_item_t *out);
void txq_clear(txq_t *q);
uint8_t txq_count(const txq_t *q);
bool txq_empty(const txq_t *q);
bool txq_full(const txq_t *q);

#endif
