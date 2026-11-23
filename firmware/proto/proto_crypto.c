/* Speck128/128 block cipher, from the NSA Simon & Speck Implementation Guide
 * 1.1 (January 2019), section 6. Words are little-endian; the first word of
 * each block is 'y' and the first key word is K[0]. All word loads use memcpy
 * so unaligned buffers and strict aliasing are safe on the RP2350.
 */

#include "proto_crypto.h"

#include <string.h>

static uint64_t load_le64(const uint8_t *p) {
    uint64_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

static void store_le64(uint8_t *p, uint64_t v) {
    memcpy(p, &v, sizeof v);
}

static uint64_t ror64(uint64_t x, unsigned r) {
    return (x >> r) | (x << (64u - r));
}

static uint64_t rol64(uint64_t x, unsigned r) {
    return (x << r) | (x >> (64u - r));
}

/* ER64(x, y, k): x = ROR(x,8) + y ^ k; y = ROL(y,3) ^ x. */
static void er64(uint64_t *x, uint64_t *y, uint64_t k) {
    *x = ror64(*x, 8u);
    *x += *y;
    *x ^= k;
    *y = rol64(*y, 3u);
    *y ^= *x;
}

void speck128_set_key(speck128_key_t *ctx,
                      const uint8_t key[SPECK128_KEY_BYTES]) {
    uint64_t a = load_le64(key);
    uint64_t b = load_le64(key + 8u);
    unsigned i = 0;

    while (i < SPECK128_ROUNDS - 1u) {
        ctx->rk[i] = a;
        er64(&b, &a, i);
        i++;
    }
    ctx->rk[i] = a;
}

void speck128_encrypt(const speck128_key_t *ctx,
                      const uint8_t in[SPECK128_BLOCK_BYTES],
                      uint8_t out[SPECK128_BLOCK_BYTES]) {
    uint64_t y = load_le64(in);
    uint64_t x = load_le64(in + 8u);
    unsigned i;

    for (i = 0; i < SPECK128_ROUNDS; i++) {
        er64(&x, &y, ctx->rk[i]);
    }

    store_le64(out, y);
    store_le64(out + 8u, x);
}

/* --- RFC 4493 CMAC over any 128-bit block primitive --- */

static const uint8_t CMAC_RB[PC_CMAC_BLOCK_BYTES] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x87
};

static void cmac_double(const uint8_t in[PC_CMAC_BLOCK_BYTES],
                        uint8_t out[PC_CMAC_BLOCK_BYTES]) {
    unsigned carry = in[0] >> 7u;
    unsigned i;

    for (i = 0; i < PC_CMAC_BLOCK_BYTES - 1u; i++) {
        out[i] = (uint8_t)((uint8_t)(in[i] << 1) | (in[i + 1u] >> 7u));
    }
    out[PC_CMAC_BLOCK_BYTES - 1u] = (uint8_t)(in[PC_CMAC_BLOCK_BYTES - 1u] << 1);
    if (carry) {
        for (i = 0; i < PC_CMAC_BLOCK_BYTES; i++) {
            out[i] ^= CMAC_RB[i];
        }
    }
}

void pc_cmac_subkeys(pc_block_fn block, const void *key_ctx,
                     uint8_t k1[PC_CMAC_BLOCK_BYTES],
                     uint8_t k2[PC_CMAC_BLOCK_BYTES]) {
    uint8_t zero[PC_CMAC_BLOCK_BYTES] = {0};
    uint8_t l[PC_CMAC_BLOCK_BYTES];

    block(key_ctx, zero, l);
    cmac_double(l, k1);
    cmac_double(k1, k2);
}

void pc_cmac(pc_block_fn block, const void *key_ctx,
             const uint8_t *msg, size_t len,
             uint8_t tag[PC_CMAC_BLOCK_BYTES]) {
    uint8_t x[PC_CMAC_BLOCK_BYTES] = {0};
    uint8_t k1[PC_CMAC_BLOCK_BYTES], k2[PC_CMAC_BLOCK_BYTES];
    uint8_t block_buf[PC_CMAC_BLOCK_BYTES];
    size_t offset = 0;

    pc_cmac_subkeys(block, key_ctx, k1, k2);

    while (len - offset > PC_CMAC_BLOCK_BYTES) {
        unsigned i;
        for (i = 0; i < PC_CMAC_BLOCK_BYTES; i++) {
            block_buf[i] = (uint8_t)(x[i] ^ msg[offset + i]);
        }
        block(key_ctx, block_buf, x);
        offset += PC_CMAC_BLOCK_BYTES;
    }

    memset(block_buf, 0, sizeof block_buf);
    if (len > offset) {
        size_t remaining = len - offset;
        memcpy(block_buf, msg + offset, remaining);
        if (remaining == PC_CMAC_BLOCK_BYTES) {
            for (unsigned i = 0; i < PC_CMAC_BLOCK_BYTES; i++) {
                block_buf[i] ^= k1[i];
            }
        } else {
            block_buf[remaining] = 0x80u;
            for (unsigned i = 0; i < PC_CMAC_BLOCK_BYTES; i++) {
                block_buf[i] ^= k2[i];
            }
        }
    } else {
        block_buf[0] = 0x80u;
        for (unsigned i = 0; i < PC_CMAC_BLOCK_BYTES; i++) {
            block_buf[i] ^= k2[i];
        }
    }

    for (unsigned i = 0; i < PC_CMAC_BLOCK_BYTES; i++) {
        block_buf[i] ^= x[i];
    }
    block(key_ctx, block_buf, tag);
}

void pc_cmac_trunc(pc_block_fn block, const void *key_ctx,
                   const uint8_t *msg, size_t len,
                   uint8_t tag[PC_TAG_BYTES]) {
    uint8_t full[PC_CMAC_BLOCK_BYTES];
    pc_cmac(block, key_ctx, msg, len, full);
    memcpy(tag, full, PC_TAG_BYTES);
}

static void speck_block_fn(const void *key_ctx,
                           const uint8_t in[PC_CMAC_BLOCK_BYTES],
                           uint8_t out[PC_CMAC_BLOCK_BYTES]) {
    speck128_encrypt((const speck128_key_t *)key_ctx, in, out);
}

void pc_kdf(const uint8_t master[PC_MASTER_KEY_BYTES],
            const uint8_t *salt, size_t salt_len,
            uint8_t label,
            uint8_t out[PC_DERIVED_KEY_BYTES]) {
    uint8_t input[64];
    speck128_key_t k1, k2;

    if (salt_len > sizeof input - 2u) {
        memset(out, 0, PC_DERIVED_KEY_BYTES);
        return;
    }

    speck128_set_key(&k1, master);
    speck128_set_key(&k2, master + SPECK128_KEY_BYTES);
    memcpy(input, salt, salt_len);
    input[salt_len] = label;
    input[salt_len + 1u] = 0x01u;
    pc_cmac(speck_block_fn, &k1, input, salt_len + 2u, out);
    input[salt_len + 1u] = 0x02u;
    pc_cmac(speck_block_fn, &k2, input, salt_len + 2u, out + PC_CMAC_BLOCK_BYTES);
}

void pc_ctr_xcrypt(const uint8_t key[SPECK128_KEY_BYTES],
                   const uint8_t nonce[PC_NONCE_BYTES],
                   uint8_t *buf, size_t len) {
    speck128_key_t sk;
    uint8_t ctr[PC_CMAC_BLOCK_BYTES], ks[PC_CMAC_BLOCK_BYTES];
    uint32_t counter = 0;
    size_t offset = 0;

    speck128_set_key(&sk, key);
    memcpy(ctr, nonce, PC_NONCE_BYTES);
    while (offset < len) {
        size_t take = len - offset;
        unsigned i;
        if (take > PC_CMAC_BLOCK_BYTES) take = PC_CMAC_BLOCK_BYTES;
        ctr[12] = (uint8_t)(counter >> 24u);
        ctr[13] = (uint8_t)(counter >> 16u);
        ctr[14] = (uint8_t)(counter >> 8u);
        ctr[15] = (uint8_t)counter;
        speck128_encrypt(&sk, ctr, ks);
        for (i = 0; i < take; i++) buf[offset + i] ^= ks[i];
        offset += take;
        counter++;
    }
}

#define PC_SUITE_MAX_AAD 64u
#define PC_SUITE_MAX_PAYLOAD 64u

static void speck_seal(const uint8_t key[SPECK128_KEY_BYTES],
                       const uint8_t nonce[PC_NONCE_BYTES],
                       const uint8_t *aad, size_t aad_len,
                       uint8_t *buf, size_t len,
                       uint8_t tag[PC_TAG_BYTES]) {
    uint8_t macbuf[PC_SUITE_MAX_AAD + PC_SUITE_MAX_PAYLOAD];
    speck128_key_t sk;

    pc_ctr_xcrypt(key, nonce, buf, len);
    speck128_set_key(&sk, key);
    memcpy(macbuf, aad, aad_len);
    memcpy(macbuf + aad_len, buf, len);
    pc_cmac_trunc(speck_block_fn, &sk, macbuf, aad_len + len, tag);
}

static int speck_open(const uint8_t key[SPECK128_KEY_BYTES],
                      const uint8_t nonce[PC_NONCE_BYTES],
                      const uint8_t *aad, size_t aad_len,
                      uint8_t *buf, size_t len,
                      const uint8_t tag[PC_TAG_BYTES]) {
    uint8_t macbuf[PC_SUITE_MAX_AAD + PC_SUITE_MAX_PAYLOAD];
    uint8_t expected[PC_TAG_BYTES];
    speck128_key_t sk;
    unsigned diff = 0;
    unsigned i;

    speck128_set_key(&sk, key);
    memcpy(macbuf, aad, aad_len);
    memcpy(macbuf + aad_len, buf, len);
    pc_cmac_trunc(speck_block_fn, &sk, macbuf, aad_len + len, expected);
    for (i = 0; i < PC_TAG_BYTES; i++) diff |= (unsigned)(expected[i] ^ tag[i]);
    if (diff != 0u) return -1; /* verify before decrypt */

    pc_ctr_xcrypt(key, nonce, buf, len);
    return 0;
}

const pc_suite_ops_t pc_suite_speck_ctr_cmac = { speck_seal, speck_open };
const pc_suite_ops_t pc_suite_aead = { NULL, NULL };

void pc_confirm_tag(const uint8_t key[SPECK128_KEY_BYTES],
                    uint8_t label,
                    uint32_t session_id,
                    uint32_t seq,
                    const uint8_t nonce[PC_CONFIRM_NONCE_BYTES],
                    uint8_t tag[PC_CONFIRM_TAG_BYTES]) {
    uint8_t input[4u + 4u + PC_CONFIRM_NONCE_BYTES + 1u];
    speck128_key_t sk;

    input[0] = (uint8_t)(session_id);
    input[1] = (uint8_t)(session_id >> 8u);
    input[2] = (uint8_t)(session_id >> 16u);
    input[3] = (uint8_t)(session_id >> 24u);
    input[4] = (uint8_t)(seq);
    input[5] = (uint8_t)(seq >> 8u);
    input[6] = (uint8_t)(seq >> 16u);
    input[7] = (uint8_t)(seq >> 24u);
    memcpy(input + 8u, nonce, PC_CONFIRM_NONCE_BYTES);
    input[16u] = label;

    speck128_set_key(&sk, key);
    pc_cmac_trunc(speck_block_fn, &sk, input, sizeof input, tag);
}

bool pc_confirm_check(const uint8_t key[SPECK128_KEY_BYTES],
                      uint8_t label,
                      uint32_t session_id,
                      uint32_t seq,
                      const uint8_t nonce[PC_CONFIRM_NONCE_BYTES],
                      const uint8_t tag[PC_CONFIRM_TAG_BYTES]) {
    uint8_t expected[PC_CONFIRM_TAG_BYTES];
    unsigned diff = 0;
    unsigned i;

    pc_confirm_tag(key, label, session_id, seq, nonce, expected);
    for (i = 0; i < PC_CONFIRM_TAG_BYTES; i++) {
        diff |= (unsigned)(expected[i] ^ tag[i]);
    }
    return diff == 0u;
}

/* Boot self-test: external Speck KAT plus a pinned Speck-CMAC tag. The CMAC
 * construction itself is validated against RFC 4493 in the host tests. */
static const uint8_t SELFTEST_KEY[16] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
};
static const uint8_t SELFTEST_PT[16] = {
    0x20,0x6d,0x61,0x64,0x65,0x20,0x69,0x74,
    0x20,0x65,0x71,0x75,0x69,0x76,0x61,0x6c
};
static const uint8_t SELFTEST_CT[16] = {
    0x18,0x0d,0x57,0x5c,0xdf,0xfe,0x60,0x78,
    0x65,0x32,0x78,0x79,0x51,0x98,0x5d,0xa6
};
static const uint8_t SELFTEST_MSG[] = { 'P','H','A','N','T','O','M' };
static const uint8_t SELFTEST_TAG[PC_TAG_BYTES] = {
    0xe0,0x67,0xb1,0x77,0xf1,0x6d,0x13,0x30
};

bool proto_crypto_self_test(void) {
    speck128_key_t sk;
    uint8_t out[16], tag[PC_TAG_BYTES];
    uint8_t master[PC_MASTER_KEY_BYTES];
    uint8_t salt[4] = { 0xde, 0xad, 0xbe, 0xef };
    uint8_t k_send[PC_DERIVED_KEY_BYTES], k_recv[PC_DERIVED_KEY_BYTES];

    speck128_set_key(&sk, SELFTEST_KEY);
    speck128_encrypt(&sk, SELFTEST_PT, out);
    if (memcmp(out, SELFTEST_CT, sizeof out) != 0) return false;

    pc_cmac_trunc(speck_block_fn, &sk, SELFTEST_MSG, sizeof SELFTEST_MSG, tag);
    if (memcmp(tag, SELFTEST_TAG, sizeof tag) != 0) return false;

    memset(master, 0, sizeof master);
    memcpy(master, SELFTEST_KEY, sizeof SELFTEST_KEY);
    pc_kdf(master, salt, sizeof salt, 0x01u, k_send);
    pc_kdf(master, salt, sizeof salt, 0x02u, k_recv);
    if (memcmp(k_send, k_recv, sizeof k_send) == 0) return false;

    return true;
}
