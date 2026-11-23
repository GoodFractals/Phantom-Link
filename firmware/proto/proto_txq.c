#include "proto_txq.h"

void txq_init(txq_t *q) {
    if (!q) return;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->enqueued = 0;
    q->sent = 0;
    q->dropped = 0;
    q->overflows = 0;
}

bool txq_enqueue(txq_t *q, const txq_item_t *item) {
    if (!q || !item) return false;
    if (q->count >= TXQ_DEPTH) {
        q->overflows++;
        return false;
    }
    q->items[q->tail] = *item;
    q->tail = (uint8_t)((q->tail + 1u) % TXQ_DEPTH);
    q->count++;
    q->enqueued++;
    return true;
}

bool txq_peek(const txq_t *q, txq_item_t *out) {
    if (!q || !out || q->count == 0) return false;
    *out = q->items[q->head];
    return true;
}

bool txq_dequeue(txq_t *q, txq_item_t *out) {
    if (!q || q->count == 0) return false;
    if (out) *out = q->items[q->head];
    q->head = (uint8_t)((q->head + 1u) % TXQ_DEPTH);
    q->count--;
    q->sent++;
    return true;
}

void txq_clear(txq_t *q) {
    if (!q) return;
    q->dropped += q->count;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

uint8_t txq_count(const txq_t *q) {
    return q ? q->count : 0;
}

bool txq_empty(const txq_t *q) {
    return !q || q->count == 0;
}

bool txq_full(const txq_t *q) {
    return q && q->count >= TXQ_DEPTH;
}
