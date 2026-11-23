#include <stdio.h>
#include <string.h>

#include "proto_txq.h"

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static void test_fifo_and_wrap(void) {
    txq_t q;
    txq_item_t item;
    int i;

    txq_init(&q);
    CHECK(txq_empty(&q));
    CHECK(txq_count(&q) == 0);
    CHECK(!txq_peek(&q, &item));
    CHECK(!txq_dequeue(&q, &item));

    for (i = 0; i < 5; i++) {
        memset(&item, 0, sizeof item);
        item.payload[0] = (uint8_t)(i + 1);
        item.payload_len = 1;
        CHECK(txq_enqueue(&q, &item));
    }
    CHECK(txq_count(&q) == 5);

    for (i = 0; i < 5; i++) {
        CHECK(txq_peek(&q, &item));
        CHECK(item.payload[0] == (uint8_t)(i + 1));
        CHECK(txq_dequeue(&q, &item));
        CHECK(item.payload[0] == (uint8_t)(i + 1));
    }
    CHECK(txq_empty(&q));
    CHECK(q.sent == 5);
    CHECK(q.enqueued == 5);

    /* Wrap around the ring boundary. */
    for (i = 0; i < (int)TXQ_DEPTH + 3; i++) {
        memset(&item, 0, sizeof item);
        item.payload[0] = (uint8_t)(i & 0xFF);
        item.payload_len = 1;
        CHECK(txq_enqueue(&q, &item));
        if (txq_count(&q) == TXQ_DEPTH) {
            CHECK(txq_dequeue(&q, NULL));
        }
    }
    CHECK(txq_count(&q) <= TXQ_DEPTH);
}

static void test_overflow_and_clear(void) {
    txq_t q;
    txq_item_t item;
    int i;

    txq_init(&q);
    memset(&item, 0, sizeof item);
    item.payload_len = 1;

    for (i = 0; i < (int)TXQ_DEPTH; i++) {
        item.payload[0] = (uint8_t)i;
        CHECK(txq_enqueue(&q, &item));
    }
    CHECK(txq_full(&q));
    CHECK(!txq_enqueue(&q, &item));
    CHECK(q.overflows == 1);
    CHECK(txq_count(&q) == TXQ_DEPTH);

    /* Overflow again keeps counting. */
    CHECK(!txq_enqueue(&q, &item));
    CHECK(q.overflows == 2);

    /* Clear drops everything and reports it. */
    txq_clear(&q);
    CHECK(txq_empty(&q));
    CHECK(txq_count(&q) == 0);
    CHECK(q.dropped == TXQ_DEPTH);
    CHECK(q.overflows == 2);

    /* The queue is usable again after a clear. */
    item.payload[0] = 99;
    CHECK(txq_enqueue(&q, &item));
    CHECK(txq_count(&q) == 1);
    CHECK(txq_peek(&q, &item) && item.payload[0] == 99);
}

static void test_null_safety(void) {
    txq_t q;
    txq_item_t item;

    txq_init(NULL);
    CHECK(!txq_enqueue(NULL, &item));
    CHECK(!txq_enqueue(&q, NULL));
    CHECK(!txq_peek(NULL, &item));
    CHECK(!txq_peek(&q, NULL));
    CHECK(!txq_dequeue(NULL, &item));
    txq_clear(NULL);
    CHECK(txq_count(NULL) == 0);
    CHECK(txq_empty(NULL));
    CHECK(!txq_full(NULL));
}

int main(void) {
    test_fifo_and_wrap();
    test_overflow_and_clear();
    test_null_safety();

    if (failures) {
        printf("txq: %d failure(s)\n", failures);
        return 1;
    }
    printf("txq: PASS\n");
    return 0;
}
