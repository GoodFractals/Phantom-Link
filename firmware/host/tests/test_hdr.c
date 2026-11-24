#include <stdio.h>
#include <string.h>

#include "proto_hdr.h"
#include "proto_crypto.h"

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static const uint8_t KEY[16] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
};
static const uint8_t WRONG_KEY[16] = {
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
};

/* magic 'B','C' | version | type=DATA | flags | payload_len=8 */
static const uint8_t CLEAR[HDR_CLEAR_BYTES] = {
    'B','C',0x01,0x03,0x00,0x08
};
/* session_id(4) | seq(4, LE = 0x12345678) | ack(4) */
static const uint8_t SECRET[HDR_SECRET_BYTES] = {
    0x11,0x22,0x33,0x44, 0x78,0x56,0x34,0x12, 0xaa,0xbb,0xcc,0xdd
};

static void test_round_trip(void) {
    uint8_t out_clear[HDR_CLEAR_BYTES], ct[HDR_SECRET_BYTES], tag[HDR_TAG_BYTES];
    uint8_t recovered[HDR_SECRET_BYTES];

    hdr_seal(KEY, CLEAR, SECRET, out_clear, ct, tag);

    /* Clear bytes pass through untouched and become the AAD. */
    CHECK(memcmp(out_clear, CLEAR, sizeof out_clear) == 0);
    /* All 12 secret bytes are encrypted, including the seq field. */
    CHECK(memcmp(ct, SECRET, sizeof ct) != 0);
    CHECK(memcmp(ct + 4u, SECRET + 4u, 4u) != 0);

    CHECK(hdr_open(KEY, out_clear, ct, SECRET + 4u, tag, recovered));
    CHECK(memcmp(recovered, SECRET, sizeof recovered) == 0);
}

/* Pin the documented nonce: 0x02 || seq(4 LE) || 0^7, seq from secret[4..7].
 * Use the primitive directly so a wrong nonce in the implementation is caught. */
static void test_nonce_kat(void) {
    static const uint8_t NONCE[PC_NONCE_BYTES] = {
        0x02,0x78,0x56,0x34,0x12, 0x00,0x00,0x00,0x00,0x00,0x00,0x00
    };
    uint8_t out_clear[HDR_CLEAR_BYTES], ct[HDR_SECRET_BYTES], tag[HDR_TAG_BYTES];
    uint8_t want[HDR_SECRET_BYTES];

    hdr_seal(KEY, CLEAR, SECRET, out_clear, ct, tag);
    memcpy(want, SECRET, sizeof want);
    pc_ctr_xcrypt(KEY, NONCE, want, sizeof want);
    CHECK(memcmp(ct, want, sizeof ct) == 0);
}

static void test_aad_tamper_rejected(void) {
    uint8_t out_clear[HDR_CLEAR_BYTES], ct[HDR_SECRET_BYTES], tag[HDR_TAG_BYTES];
    uint8_t bad_clear[HDR_CLEAR_BYTES], recovered[HDR_SECRET_BYTES];
    size_t i;
    unsigned bit;

    hdr_seal(KEY, CLEAR, SECRET, out_clear, ct, tag);
    for (i = 0u; i < sizeof bad_clear; i++) {
        for (bit = 0u; bit < 8u; bit++) {
            memcpy(bad_clear, out_clear, sizeof bad_clear);
            bad_clear[i] ^= (uint8_t)(1u << bit);
            CHECK(!hdr_open(KEY, bad_clear, ct, SECRET + 4u, tag, recovered));
        }
    }
}

static void test_ciphertext_tamper_rejected(void) {
    uint8_t out_clear[HDR_CLEAR_BYTES], ct[HDR_SECRET_BYTES], tag[HDR_TAG_BYTES];
    uint8_t bad_ct[HDR_SECRET_BYTES], recovered[HDR_SECRET_BYTES];
    size_t i;
    unsigned bit;

    hdr_seal(KEY, CLEAR, SECRET, out_clear, ct, tag);
    for (i = 0u; i < sizeof bad_ct; i++) {
        for (bit = 0u; bit < 8u; bit++) {
            memcpy(bad_ct, ct, sizeof bad_ct);
            bad_ct[i] ^= (uint8_t)(1u << bit);
            CHECK(!hdr_open(KEY, out_clear, bad_ct, SECRET + 4u, tag, recovered));
        }
    }
}

static void test_tag_bit_flip_rejected(void) {
    uint8_t out_clear[HDR_CLEAR_BYTES], ct[HDR_SECRET_BYTES], tag[HDR_TAG_BYTES];
    uint8_t bad_tag[HDR_TAG_BYTES], recovered[HDR_SECRET_BYTES];
    size_t i;
    unsigned bit;

    hdr_seal(KEY, CLEAR, SECRET, out_clear, ct, tag);
    for (i = 0u; i < sizeof bad_tag; i++) {
        for (bit = 0u; bit < 8u; bit++) {
            memcpy(bad_tag, tag, sizeof bad_tag);
            bad_tag[i] ^= (uint8_t)(1u << bit);
            CHECK(!hdr_open(KEY, out_clear, ct, SECRET + 4u, bad_tag, recovered));
        }
    }
}

static void test_wrong_key_rejected(void) {
    uint8_t out_clear[HDR_CLEAR_BYTES], ct[HDR_SECRET_BYTES], tag[HDR_TAG_BYTES];
    uint8_t recovered[HDR_SECRET_BYTES];

    hdr_seal(KEY, CLEAR, SECRET, out_clear, ct, tag);
    CHECK(!hdr_open(WRONG_KEY, out_clear, ct, SECRET + 4u, tag, recovered));
    CHECK(!hdr_open(KEY, out_clear, ct, WRONG_KEY + 4u, tag, recovered));
}

static void test_recovered_unchanged_on_failure(void) {
    static const uint8_t SENTINEL[HDR_SECRET_BYTES] = {
        0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a
    };
    uint8_t out_clear[HDR_CLEAR_BYTES], ct[HDR_SECRET_BYTES], tag[HDR_TAG_BYTES];
    uint8_t recovered[HDR_SECRET_BYTES];
    uint8_t bad[HDR_TAG_BYTES];

    hdr_seal(KEY, CLEAR, SECRET, out_clear, ct, tag);
    memcpy(bad, tag, sizeof bad);
    bad[0] ^= 0x01u;

    memcpy(recovered, SENTINEL, sizeof recovered);
    CHECK(!hdr_open(KEY, out_clear, ct, SECRET + 4u, bad, recovered));
    CHECK(memcmp(recovered, SENTINEL, sizeof recovered) == 0);

    memcpy(recovered, SENTINEL, sizeof recovered);
    CHECK(!hdr_open(WRONG_KEY, out_clear, ct, SECRET + 4u, tag, recovered));
    CHECK(memcmp(recovered, SENTINEL, sizeof recovered) == 0);

    memcpy(recovered, SENTINEL, sizeof recovered);
    CHECK(!hdr_open(KEY, out_clear, ct, WRONG_KEY + 4u, tag, recovered));
    CHECK(memcmp(recovered, SENTINEL, sizeof recovered) == 0);

    /* A valid frame immediately after the rejections still opens. */
    CHECK(hdr_open(KEY, out_clear, ct, SECRET + 4u, tag, recovered));
    CHECK(memcmp(recovered, SECRET, sizeof recovered) == 0);
}

int main(void) {
    test_round_trip();
    test_nonce_kat();
    test_aad_tamper_rejected();
    test_ciphertext_tamper_rejected();
    test_tag_bit_flip_rejected();
    test_wrong_key_rejected();
    test_recovered_unchanged_on_failure();

    if (failures) {
        printf("hdr: %d failure(s)\n", failures);
        return 1;
    }
    printf("hdr: PASS\n");
    return 0;
}
