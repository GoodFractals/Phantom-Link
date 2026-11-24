#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "proto_crypto.h"
#include "proto_ratchet.h"

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

/* Master is 0x00..0x1f. KATs are pc_kdf(master, LE32(msg_id), 0x04). */
static const uint8_t MASTER[PC_MASTER_KEY_BYTES] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
};
static const uint8_t KAT_12345678[PC_DERIVED_KEY_BYTES] = {
    0x28,0x48,0x89,0xc2,0xdc,0xfe,0x52,0x0e,
    0xe8,0x65,0xf9,0x3e,0x56,0xe5,0x3a,0xdf,
    0x67,0x76,0xda,0x27,0x72,0x74,0x63,0xee,
    0x69,0x42,0x43,0xd7,0xf5,0xc9,0xbb,0xa5
};
static const uint8_t KAT_ID_1[PC_DERIVED_KEY_BYTES] = {
    0xb4,0x89,0xb0,0xa1,0x03,0x7e,0x21,0xaa,
    0x7b,0xbe,0xb2,0xba,0xbe,0x93,0x67,0x6f,
    0x81,0x52,0x86,0xac,0x86,0xde,0x6d,0x3f,
    0x9f,0x61,0x96,0x6d,0x34,0xd3,0xeb,0xd1
};
static const uint8_t KAT_ID_2[PC_DERIVED_KEY_BYTES] = {
    0xd7,0x37,0xb3,0x7d,0xc4,0xd5,0xa6,0x64,
    0x19,0x68,0xd3,0x80,0x45,0xa1,0x7a,0x13,
    0xaf,0x2c,0x09,0x0c,0xcd,0x57,0xb3,0xa4,
    0xb9,0xfe,0x73,0x4a,0x4a,0x6c,0x67,0xe4
};

static void test_init(void) {
    ratchet_t st;
    unsigned i;

    memset(&st, 0xa5, sizeof st);
    ratchet_init(&st, MASTER);
    CHECK(st.ready);
    CHECK(!st.has_tip);
    CHECK(st.tip == 0u);
    CHECK(st.accepted == 0u);
    CHECK(st.rollbacks_rejected == 0u);
    CHECK(memcmp(st.master_key, MASTER, sizeof MASTER) == 0);
    for (i = 0; i < RATCHET_WINDOW; i++) CHECK(!st.used[i]);
}

static void test_kat_and_determinism(void) {
    ratchet_t a, b;
    uint8_t k1[PC_DERIVED_KEY_BYTES], k2[PC_DERIVED_KEY_BYTES];
    uint8_t salt[RATCHET_MSG_ID_BYTES] = { 0x78u, 0x56u, 0x34u, 0x12u };
    uint8_t direct[PC_DERIVED_KEY_BYTES];
    volatile uint8_t label = RATCHET_LABEL;

    ratchet_init(&a, MASTER);
    ratchet_init(&b, MASTER);
    CHECK(ratchet_advance(&a, 0x12345678u, k1));
    CHECK(ratchet_advance(&b, 0x12345678u, k2));
    CHECK(memcmp(k1, k2, sizeof k1) == 0);
    CHECK(memcmp(k1, KAT_12345678, sizeof k1) == 0);

    CHECK(ratchet_key_for(&a, 0x12345678u, k2));
    CHECK(memcmp(k1, k2, sizeof k1) == 0);

    pc_kdf(MASTER, salt, sizeof salt, RATCHET_LABEL, direct);
    CHECK(memcmp(k1, direct, sizeof k1) == 0);
    CHECK(label == 0x04u);
}

static void test_distinct_ids_distinct_keys(void) {
    ratchet_t st;
    uint8_t k1[PC_DERIVED_KEY_BYTES], k2[PC_DERIVED_KEY_BYTES];
    uint8_t k3[PC_DERIVED_KEY_BYTES];

    ratchet_init(&st, MASTER);
    CHECK(ratchet_advance(&st, 1u, k1));
    CHECK(ratchet_advance(&st, 2u, k2));
    CHECK(ratchet_advance(&st, 3u, k3));
    CHECK(memcmp(k1, k2, sizeof k1) != 0);
    CHECK(memcmp(k2, k3, sizeof k2) != 0);
    CHECK(memcmp(k1, k3, sizeof k1) != 0);
    CHECK(memcmp(k1, KAT_ID_1, sizeof k1) == 0);
    CHECK(memcmp(k2, KAT_ID_2, sizeof k2) == 0);
}

static void test_out_of_order_within_window(void) {
    ratchet_t st;
    uint8_t saved[RATCHET_WINDOW][PC_DERIVED_KEY_BYTES];
    uint8_t out[PC_DERIVED_KEY_BYTES];
    unsigned i;

    ratchet_init(&st, MASTER);
    for (i = 0; i < RATCHET_WINDOW; i++) {
        CHECK(ratchet_advance(&st, 100u + (uint32_t)i, saved[i]));
    }
    CHECK(st.tip == 107u);

    /* Reverse delivery order: every id in the window still decrypts. */
    for (i = RATCHET_WINDOW; i > 0u; i--) {
        CHECK(ratchet_key_for(&st, 100u + (uint32_t)(i - 1u), out));
        CHECK(memcmp(out, saved[i - 1u], sizeof out) == 0);
    }

    /* A late/duplicate advance yields the cached key without moving the tip. */
    CHECK(ratchet_advance(&st, 103u, out));
    CHECK(memcmp(out, saved[3u], sizeof out) == 0);
    CHECK(st.tip == 107u);
    CHECK(ratchet_advance(&st, 107u, out));
    CHECK(memcmp(out, saved[7u], sizeof out) == 0);
    CHECK(st.tip == 107u);
}

static void test_rollback_beyond_window(void) {
    ratchet_t st;
    uint8_t out[PC_DERIVED_KEY_BYTES];
    unsigned i;

    ratchet_init(&st, MASTER);
    for (i = 0; i < RATCHET_WINDOW; i++) {
        CHECK(ratchet_advance(&st, 100u + (uint32_t)i, out));
    }

    memset(out, 0xcc, sizeof out);
    CHECK(!ratchet_key_for(&st, 99u, out));
    for (i = 0; i < sizeof out; i++) CHECK(out[i] == 0u);
    CHECK(st.rollbacks_rejected == 1u);

    CHECK(!ratchet_advance(&st, 99u, out));
    CHECK(st.rollbacks_rejected == 2u);
    CHECK(st.tip == 107u);

    /* The oldest in-window id is still valid; an unknown future id is not. */
    CHECK(ratchet_key_for(&st, 100u, out));
    CHECK(!ratchet_key_for(&st, 0xdeadbeefu, out));
    CHECK(st.rollbacks_rejected == 3u);
}

static void test_window_wraparound(void) {
    static const uint32_t ids[5] = {
        0xfffffffeu, 0xffffffffu, 0u, 1u, 2u
    };
    ratchet_t st;
    uint8_t saved[5][PC_DERIVED_KEY_BYTES];
    uint8_t out[PC_DERIVED_KEY_BYTES];
    unsigned i;

    ratchet_init(&st, MASTER);
    for (i = 0; i < 5u; i++) {
        CHECK(ratchet_advance(&st, ids[i], saved[i]));
    }
    CHECK(st.tip == 2u);
    for (i = 0; i < 5u; i++) {
        CHECK(ratchet_key_for(&st, ids[i], out));
        CHECK(memcmp(out, saved[i], sizeof out) == 0);
    }
    CHECK(!ratchet_key_for(&st, 0xfffffff9u, out));

    /* Sliding past the wrap keeps still-in-window ids decryptable. */
    CHECK(ratchet_advance(&st, 4u, out));
    CHECK(st.tip == 4u);
    CHECK(ratchet_key_for(&st, 0xfffffffeu, out));
    CHECK(memcmp(out, saved[0u], sizeof out) == 0);
    CHECK(!ratchet_key_for(&st, 0xfffffff9u, out));
}

static void test_null_safety(void) {
    ratchet_t st;
    uint8_t out[PC_DERIVED_KEY_BYTES];
    unsigned i;

    ratchet_init(NULL, MASTER);
    CHECK(!ratchet_advance(NULL, 1u, out));
    CHECK(!ratchet_key_for(NULL, 1u, out));

    ratchet_init(&st, MASTER);
    CHECK(!ratchet_advance(&st, 1u, NULL));
    CHECK(!ratchet_key_for(&st, 1u, NULL));
    CHECK(!st.has_tip);

    ratchet_init(&st, NULL);
    CHECK(!st.ready);
    memset(out, 0xcc, sizeof out);
    CHECK(!ratchet_advance(&st, 1u, out));
    for (i = 0; i < sizeof out; i++) CHECK(out[i] == 0u);
    CHECK(!ratchet_key_for(&st, 1u, out));

    ratchet_init(&st, MASTER);
    memset(out, 0xcc, sizeof out);
    CHECK(!ratchet_key_for(&st, 1u, out));
    for (i = 0; i < sizeof out; i++) CHECK(out[i] == 0u);
}

int main(void) {
    test_init();
    test_kat_and_determinism();
    test_distinct_ids_distinct_keys();
    test_out_of_order_within_window();
    test_rollback_beyond_window();
    test_window_wraparound();
    test_null_safety();

    if (failures) {
        printf("ratchet: %d failure(s)\n", failures);
        return 1;
    }
    printf("ratchet: PASS\n");
    return 0;
}
