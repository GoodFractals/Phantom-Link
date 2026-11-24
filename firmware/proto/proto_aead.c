/* ChaCha20-Poly1305 AEAD for Phantom Link (ticket 32).
 *
 * RFC 8439 section 2.3 (ChaCha20 block function), 2.5 (Poly1305),
 * 2.6 (Poly1305 key generation) and 2.8 (AEAD construction).  The only
 * deviation from RFC 8439 is that the 16-byte Poly1305 tag is truncated to
 * its first 8 bytes (MSB-first), matching the 8-byte frame auth field.
 *
 * host-only / SDK-free, C11, no dynamic allocation.  The Poly1305 core uses
 * the constant-time 26-bit limb approach from poly1305-donna (public domain).
 */

#include "proto_aead.h"

#include <string.h>

/* ------------------------------------------------------------------------- */
/* Small endian helpers (RP2350 and the host are little-endian; these are     */
/* written as explicit byte shifts so they are correct on any host).          */
/* ------------------------------------------------------------------------- */
static uint32_t load_le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void store_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void store_le64(uint8_t p[8], uint64_t v) {
    unsigned i;
    for (i = 0u; i < 8u; i++) p[i] = (uint8_t)(v >> (8u * i));
}

/* ------------------------------------------------------------------------- */
/* ChaCha20 block function (RFC 8439 section 2.3)                             */
/* ------------------------------------------------------------------------- */
static uint32_t rotl32(uint32_t x, unsigned n) {
    return (x << n) | (x >> (32u - n));
}

#define CHACHA_QR(a, b, c, d)                                                  \
    do {                                                                       \
        (a) += (b); (d) ^= (a); (d) = rotl32((d), 16u);                        \
        (c) += (d); (b) ^= (c); (b) = rotl32((b), 12u);                        \
        (a) += (b); (d) ^= (a); (d) = rotl32((d), 8u);                         \
        (c) += (d); (b) ^= (c); (b) = rotl32((b), 7u);                         \
    } while (0)

void chacha20_block(const uint8_t key[CHACHA20_KEY_BYTES], uint32_t counter,
                    const uint8_t nonce[CHACHA20_NONCE_BYTES],
                    uint8_t out[CHACHA20_BLOCK_BYTES]) {
    static const uint32_t CONST[4] = {
        0x61707865u, 0x3320646eu, 0x79622d32u, 0x6b206574u
    };
    uint32_t st[16];
    uint32_t x[16];
    unsigned i;

    st[0] = CONST[0]; st[1] = CONST[1];
    st[2] = CONST[2]; st[3] = CONST[3];
    for (i = 0u; i < 8u; i++) st[4u + i] = load_le32(key + 4u * i);
    st[12] = counter;
    for (i = 0u; i < 3u; i++) st[13u + i] = load_le32(nonce + 4u * i);

    memcpy(x, st, sizeof x);
    for (i = 0u; i < 10u; i++) {
        CHACHA_QR(x[0], x[4], x[8],  x[12]);
        CHACHA_QR(x[1], x[5], x[9],  x[13]);
        CHACHA_QR(x[2], x[6], x[10], x[14]);
        CHACHA_QR(x[3], x[7], x[11], x[15]);
        CHACHA_QR(x[0], x[5], x[10], x[15]);
        CHACHA_QR(x[1], x[6], x[11], x[12]);
        CHACHA_QR(x[2], x[7], x[8],  x[13]);
        CHACHA_QR(x[3], x[4], x[9],  x[14]);
    }
    for (i = 0u; i < 16u; i++) {
        store_le32(out + 4u * i, x[i] + st[i]);
    }
}

/* XOR a buffer with the ChaCha20 keystream starting at `counter`.  Used for
 * both encryption (seal) and decryption (open): the operation is its own
 * inverse. */
static void chacha20_xcrypt(const uint8_t key[AEAD_KEY_BYTES], uint32_t counter,
                            const uint8_t nonce[AEAD_NONCE_BYTES],
                            uint8_t *buf, size_t len) {
    uint8_t ks[CHACHA20_BLOCK_BYTES];
    size_t offset = 0u;

    while (offset < len) {
        size_t take = len - offset;
        size_t i;
        if (take > CHACHA20_BLOCK_BYTES) take = CHACHA20_BLOCK_BYTES;
        chacha20_block(key, counter, nonce, ks);
        for (i = 0u; i < take; i++) buf[offset + i] ^= ks[i];
        offset += take;
        counter++;
    }
}

/* ------------------------------------------------------------------------- */
/* Poly1305 MAC (RFC 8439 section 2.5)                                        */
/* ------------------------------------------------------------------------- */
typedef struct {
    uint32_t r[5];      /* clamped key, 26-bit limbs */
    uint32_t h[5];      /* accumulator, 26-bit limbs */
    uint32_t pad[4];    /* "s", little-endian words */
    uint8_t  buf[16];
    size_t   buflen;
    int      final;     /* set before the last partial block: hibit = 0 */
} poly1305_ctx_t;

static void poly1305_init(poly1305_ctx_t *ctx, const uint8_t key[32]) {
    uint32_t t0 = load_le32(key + 0u);
    uint32_t t1 = load_le32(key + 4u);
    uint32_t t2 = load_le32(key + 8u);
    uint32_t t3 = load_le32(key + 12u);
    unsigned i;

    /* r &= 0x0ffffffc0ffffffc0ffffffc0fffffff */
    ctx->r[0] = (t0                    ) & 0x3ffffffu;
    ctx->r[1] = ((t0 >> 26) | (t1 << 6) ) & 0x3ffff03u;
    ctx->r[2] = ((t1 >> 20) | (t2 << 12)) & 0x3ffc0ffu;
    ctx->r[3] = ((t2 >> 14) | (t3 << 18)) & 0x3f03fffu;
    ctx->r[4] = (t3 >> 8)                & 0x00fffffu;

    ctx->pad[0] = load_le32(key + 16u);
    ctx->pad[1] = load_le32(key + 20u);
    ctx->pad[2] = load_le32(key + 24u);
    ctx->pad[3] = load_le32(key + 28u);

    for (i = 0u; i < 5u; i++) ctx->h[i] = 0u;
    ctx->buflen = 0u;
    ctx->final = 0;
}

static void poly1305_blocks(poly1305_ctx_t *ctx, const uint8_t *m, size_t bytes) {
    const uint32_t hibit = ctx->final ? 0u : (1u << 24); /* 1 << 128 */
    uint32_t r0 = ctx->r[0], r1 = ctx->r[1], r2 = ctx->r[2];
    uint32_t r3 = ctx->r[3], r4 = ctx->r[4];
    uint32_t s1 = r1 * 5u, s2 = r2 * 5u, s3 = r3 * 5u, s4 = r4 * 5u;
    uint32_t h0 = ctx->h[0], h1 = ctx->h[1], h2 = ctx->h[2];
    uint32_t h3 = ctx->h[3], h4 = ctx->h[4];
    uint64_t d0, d1, d2, d3, d4;
    uint32_t c;

    while (bytes >= 16u) {
        uint32_t t0 = load_le32(m + 0u);
        uint32_t t1 = load_le32(m + 4u);
        uint32_t t2 = load_le32(m + 8u);
        uint32_t t3 = load_le32(m + 12u);

        /* h += block + (1 << bit-length) */
        h0 += (t0                     ) & 0x3ffffffu;
        h1 += ((t0 >> 26) | (t1 << 6) ) & 0x3ffffffu;
        h2 += ((t1 >> 20) | (t2 << 12)) & 0x3ffffffu;
        h3 += ((t2 >> 14) | (t3 << 18)) & 0x3ffffffu;
        h4 += (t3 >> 8) | hibit;

        /* h *= r (mod 2^130 - 5) */
        d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 +
             (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
        d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 +
             (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
        d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 +
             (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
        d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 +
             (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
        d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 +
             (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

        c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffffu; d1 += c;
        c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffffu; d2 += c;
        c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffffu; d3 += c;
        c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffffu; d4 += c;
        c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffffu; h0 += c * 5u;
        c = h0 >> 26; h0 &= 0x3ffffffu; h1 += c;

        m += 16u;
        bytes -= 16u;
    }

    ctx->h[0] = h0; ctx->h[1] = h1; ctx->h[2] = h2;
    ctx->h[3] = h3; ctx->h[4] = h4;
}

static void poly1305_update(poly1305_ctx_t *ctx, const uint8_t *m, size_t len) {
    if (ctx->buflen != 0u) {
        size_t want = 16u - ctx->buflen;
        if (want > len) want = len;
        memcpy(ctx->buf + ctx->buflen, m, want);
        ctx->buflen += want;
        m += want;
        len -= want;
        if (ctx->buflen == 16u) {
            poly1305_blocks(ctx, ctx->buf, 16u);
            ctx->buflen = 0u;
        }
    }
    if (len >= 16u) {
        size_t whole = len - (len % 16u);
        poly1305_blocks(ctx, m, whole);
        m += whole;
        len -= whole;
    }
    if (len != 0u) {
        memcpy(ctx->buf, m, len);
        ctx->buflen = len;
    }
}

static void poly1305_finish(poly1305_ctx_t *ctx, uint8_t tag[16]) {
    uint32_t h0 = ctx->h[0], h1 = ctx->h[1], h2 = ctx->h[2];
    uint32_t h3 = ctx->h[3], h4 = ctx->h[4];
    uint32_t g0, g1, g2, g3, g4, c;
    uint32_t w0, w1, w2, w3, carry;
    uint64_t f;

    if (ctx->buflen != 0u) {
        ctx->buf[ctx->buflen++] = 0x01u;
        memset(ctx->buf + ctx->buflen, 0, 16u - ctx->buflen);
        ctx->final = 1;
        poly1305_blocks(ctx, ctx->buf, 16u);
        h0 = ctx->h[0]; h1 = ctx->h[1]; h2 = ctx->h[2];
        h3 = ctx->h[3]; h4 = ctx->h[4];
    }

    /* fully carry h */
    c = h1 >> 26; h1 &= 0x3ffffffu; h2 += c;
    c = h2 >> 26; h2 &= 0x3ffffffu; h3 += c;
    c = h3 >> 26; h3 &= 0x3ffffffu; h4 += c;
    c = h4 >> 26; h4 &= 0x3ffffffu; h0 += c * 5u;
    c = h0 >> 26; h0 &= 0x3ffffffu; h1 += c;

    /* compute h + -p */
    g0 = h0 + 5u; c = g0 >> 26; g0 &= 0x3ffffffu;
    g1 = h1 + c;  c = g1 >> 26; g1 &= 0x3ffffffu;
    g2 = h2 + c;  c = g2 >> 26; g2 &= 0x3ffffffu;
    g3 = h3 + c;  c = g3 >> 26; g3 &= 0x3ffffffu;
    g4 = h4 + c - (1u << 26);

    /* select h if h < p, else h + -p (constant time) */
    c = (g4 >> 31) - 1u;
    g0 &= c; g1 &= c; g2 &= c; g3 &= c;
    c = ~c;
    h0 = (h0 & c) | g0;
    h1 = (h1 & c) | g1;
    h2 = (h2 & c) | g2;
    h3 = (h3 & c) | g3;
    h4 = h4 & c;

    /* First reinterpret the reduced 130-bit accumulator (five 26-bit limbs)
     * as four 32-bit words, then add s (mod 2^128).  Adding s to the 26-bit
     * limbs before recombination would overlap the 6-bit word boundaries. */
    w0 = h0 | (h1 << 26);
    h1 >>= 6;
    w1 = h1 | (h2 << 20);
    h2 >>= 12;
    w2 = h2 | (h3 << 14);
    h3 >>= 18;
    w3 = h3 | (h4 << 8);

    f = (uint64_t)w0 + ctx->pad[0];
    w0 = (uint32_t)f; carry = (uint32_t)(f >> 32);
    f = (uint64_t)w1 + ctx->pad[1] + carry;
    w1 = (uint32_t)f; carry = (uint32_t)(f >> 32);
    f = (uint64_t)w2 + ctx->pad[2] + carry;
    w2 = (uint32_t)f; carry = (uint32_t)(f >> 32);
    f = (uint64_t)w3 + ctx->pad[3] + carry; /* final carry discarded: mod 2^128 */
    w3 = (uint32_t)f;

    store_le32(tag + 0u,  w0);
    store_le32(tag + 4u,  w1);
    store_le32(tag + 8u,  w2);
    store_le32(tag + 12u, w3);
}

void poly1305(const uint8_t key[POLY1305_KEY_BYTES],
              const uint8_t *msg, size_t len,
              uint8_t tag[POLY1305_TAG_BYTES]) {
    poly1305_ctx_t ctx;
    poly1305_init(&ctx, key);
    if (len != 0u) poly1305_update(&ctx, msg, len);
    poly1305_finish(&ctx, tag);
}

/* ------------------------------------------------------------------------- */
/* AEAD construction (RFC 8439 section 2.8), 8-byte truncated tag             */
/* ------------------------------------------------------------------------- */
static int ct_equal(const uint8_t a[AEAD_TAG_BYTES],
                    const uint8_t b[AEAD_TAG_BYTES]) {
    uint8_t diff = 0u;
    unsigned i;
    for (i = 0u; i < AEAD_TAG_BYTES; i++) {
        diff = (uint8_t)(diff | (uint8_t)(a[i] ^ b[i]));
    }
    return diff == 0u;
}

/* Zero-pad to a 16-byte boundary (RFC 8439 pad16). */
static void aead_pad16(poly1305_ctx_t *ctx, size_t len) {
    static const uint8_t zeros[16] = { 0 };
    size_t pad = (16u - (len % 16u)) % 16u;
    if (pad != 0u) poly1305_update(ctx, zeros, pad);
}

static void aead_tag(const uint8_t key[AEAD_KEY_BYTES],
                     const uint8_t nonce[AEAD_NONCE_BYTES],
                     const uint8_t *aad, size_t aad_len,
                     const uint8_t *ct, size_t ct_len,
                     uint8_t tag[AEAD_TAG_BYTES]) {
    uint8_t otk_block[CHACHA20_BLOCK_BYTES];
    uint8_t fulltag[POLY1305_TAG_BYTES];
    uint8_t lengths[16];
    poly1305_ctx_t ctx;

    /* Poly1305 one-time key = first 32 bytes of ChaCha20 block(counter=0). */
    chacha20_block(key, 0u, nonce, otk_block);
    poly1305_init(&ctx, otk_block);

    if (aad_len != 0u) poly1305_update(&ctx, aad, aad_len);
    aead_pad16(&ctx, aad_len);
    if (ct_len != 0u) poly1305_update(&ctx, ct, ct_len);
    aead_pad16(&ctx, ct_len);
    store_le64(lengths,      (uint64_t)aad_len);
    store_le64(lengths + 8u, (uint64_t)ct_len);
    poly1305_update(&ctx, lengths, sizeof lengths);
    poly1305_finish(&ctx, fulltag);

    /* MSB-first truncation into the 8-byte frame auth field. */
    memcpy(tag, fulltag, AEAD_TAG_BYTES);
}

void aead_seal(const uint8_t key[AEAD_KEY_BYTES],
               const uint8_t nonce[AEAD_NONCE_BYTES],
               const uint8_t *aad, size_t aad_len,
               uint8_t *buf, size_t len,
               uint8_t tag[AEAD_TAG_BYTES]) {
    chacha20_xcrypt(key, 1u, nonce, buf, len);   /* encrypt-then-MAC */
    aead_tag(key, nonce, aad, aad_len, buf, len, tag);
}

int aead_open(const uint8_t key[AEAD_KEY_BYTES],
              const uint8_t nonce[AEAD_NONCE_BYTES],
              const uint8_t *aad, size_t aad_len,
              uint8_t *buf, size_t len,
              const uint8_t tag[AEAD_TAG_BYTES]) {
    uint8_t expected[AEAD_TAG_BYTES];

    aead_tag(key, nonce, aad, aad_len, buf, len, expected);
    if (!ct_equal(expected, tag)) return -1;     /* verify before decrypting */
    chacha20_xcrypt(key, 1u, nonce, buf, len);
    return 0;
}

const aead_suite_ops_t aead_suite_chacha20_poly1305 = { aead_seal, aead_open };

/* ------------------------------------------------------------------------- */
/* Self-test / KAT runner (RFC 8439 published vectors).                       */
/* ------------------------------------------------------------------------- */
static const uint8_t ST_CHACHA_KEY[CHACHA20_KEY_BYTES] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
};
static const uint8_t ST_CHACHA_NONCE[CHACHA20_NONCE_BYTES] = {
    0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x4a,0x00,0x00,0x00,0x00
};
static const uint8_t ST_CHACHA_WANT[CHACHA20_BLOCK_BYTES] = {
    0x10,0xf1,0xe7,0xe4,0xd1,0x3b,0x59,0x15,
    0x50,0x0f,0xdd,0x1f,0xa3,0x20,0x71,0xc4,
    0xc7,0xd1,0xf4,0xc7,0x33,0xc0,0x68,0x03,
    0x04,0x22,0xaa,0x9a,0xc3,0xd4,0x6c,0x4e,
    0xd2,0x82,0x64,0x46,0x07,0x9f,0xaa,0x09,
    0x14,0xc2,0xd7,0x05,0xd9,0x8b,0x02,0xa2,
    0xb5,0x12,0x9c,0xd1,0xde,0x16,0x4e,0xb9,
    0xcb,0xd0,0x83,0xe8,0xa2,0x50,0x3c,0x4e
};

static const uint8_t ST_POLY_KEY[POLY1305_KEY_BYTES] = {
    0x85,0xd6,0xbe,0x78,0x57,0x55,0x6d,0x33,
    0x7f,0x44,0x52,0xfe,0x42,0xd5,0x06,0xa8,
    0x01,0x03,0x80,0x8a,0xfb,0x0d,0xb2,0xfd,
    0x4a,0xbf,0xf6,0xaf,0x41,0x49,0xf5,0x1b
};
static const uint8_t ST_POLY_MSG[34] = {
    0x43,0x72,0x79,0x70,0x74,0x6f,0x67,0x72,
    0x61,0x70,0x68,0x69,0x63,0x20,0x46,0x6f,
    0x72,0x75,0x6d,0x20,0x52,0x65,0x73,0x65,
    0x61,0x72,0x63,0x68,0x20,0x47,0x72,0x6f,
    0x75,0x70
};
static const uint8_t ST_POLY_WANT[POLY1305_TAG_BYTES] = {
    0xa8,0x06,0x1d,0xc1,0x30,0x51,0x36,0xc6,
    0xc2,0x2b,0x8b,0xaf,0x0c,0x01,0x27,0xa9
};

static const uint8_t ST_AEAD_KEY[AEAD_KEY_BYTES] = {
    0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
    0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
    0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f
};
static const uint8_t ST_AEAD_NONCE[AEAD_NONCE_BYTES] = {
    0x07,0x00,0x00,0x00,0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47
};
static const uint8_t ST_AEAD_AAD[12] = {
    0x50,0x51,0x52,0x53,0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7
};
static const uint8_t ST_AEAD_PT[114] = {
    0x4c,0x61,0x64,0x69,0x65,0x73,0x20,0x61,
    0x6e,0x64,0x20,0x47,0x65,0x6e,0x74,0x6c,
    0x65,0x6d,0x65,0x6e,0x20,0x6f,0x66,0x20,
    0x74,0x68,0x65,0x20,0x63,0x6c,0x61,0x73,
    0x73,0x20,0x6f,0x66,0x20,0x27,0x39,0x39,
    0x3a,0x20,0x49,0x66,0x20,0x49,0x20,0x63,
    0x6f,0x75,0x6c,0x64,0x20,0x6f,0x66,0x66,
    0x65,0x72,0x20,0x79,0x6f,0x75,0x20,0x6f,
    0x6e,0x6c,0x79,0x20,0x6f,0x6e,0x65,0x20,
    0x74,0x69,0x70,0x20,0x66,0x6f,0x72,0x20,
    0x74,0x68,0x65,0x20,0x66,0x75,0x74,0x75,
    0x72,0x65,0x2c,0x20,0x73,0x75,0x6e,0x73,
    0x63,0x72,0x65,0x65,0x6e,0x20,0x77,0x6f,
    0x75,0x6c,0x64,0x20,0x62,0x65,0x20,0x69,
    0x74,0x2e
};
static const uint8_t ST_AEAD_CT[sizeof ST_AEAD_PT] = {
    0xd3,0x1a,0x8d,0x34,0x64,0x8e,0x60,0xdb,
    0x7b,0x86,0xaf,0xbc,0x53,0xef,0x7e,0xc2,
    0xa4,0xad,0xed,0x51,0x29,0x6e,0x08,0xfe,
    0xa9,0xe2,0xb5,0xa7,0x36,0xee,0x62,0xd6,
    0x3d,0xbe,0xa4,0x5e,0x8c,0xa9,0x67,0x12,
    0x82,0xfa,0xfb,0x69,0xda,0x92,0x72,0x8b,
    0x1a,0x71,0xde,0x0a,0x9e,0x06,0x0b,0x29,
    0x05,0xd6,0xa5,0xb6,0x7e,0xcd,0x3b,0x36,
    0x92,0xdd,0xbd,0x7f,0x2d,0x77,0x8b,0x8c,
    0x98,0x03,0xae,0xe3,0x28,0x09,0x1b,0x58,
    0xfa,0xb3,0x24,0xe4,0xfa,0xd6,0x75,0x94,
    0x55,0x85,0x80,0x8b,0x48,0x31,0xd7,0xbc,
    0x3f,0xf4,0xde,0xf0,0x8e,0x4b,0x7a,0x9d,
    0xe5,0x76,0xd2,0x65,0x86,0xce,0xc6,0x4b,
    0x61,0x16
};
static const uint8_t ST_AEAD_TAG8[AEAD_TAG_BYTES] = {
    0x1a,0xe1,0x0b,0x59,0x4f,0x09,0xe2,0x6a
};

bool aead_self_test(void) {
    uint8_t out[CHACHA20_BLOCK_BYTES];
    uint8_t tag[POLY1305_TAG_BYTES];
    uint8_t short_tag[AEAD_TAG_BYTES];
    uint8_t buf[sizeof ST_AEAD_PT];

    chacha20_block(ST_CHACHA_KEY, 1u, ST_CHACHA_NONCE, out);
    if (memcmp(out, ST_CHACHA_WANT, sizeof out) != 0) return false;

    poly1305(ST_POLY_KEY, ST_POLY_MSG, sizeof ST_POLY_MSG, tag);
    if (memcmp(tag, ST_POLY_WANT, sizeof tag) != 0) return false;

    memcpy(buf, ST_AEAD_PT, sizeof buf);
    aead_seal(ST_AEAD_KEY, ST_AEAD_NONCE,
              ST_AEAD_AAD, sizeof ST_AEAD_AAD,
              buf, sizeof buf, short_tag);
    if (memcmp(buf, ST_AEAD_CT, sizeof buf) != 0) return false;
    if (memcmp(short_tag, ST_AEAD_TAG8, sizeof short_tag) != 0) return false;

    if (aead_open(ST_AEAD_KEY, ST_AEAD_NONCE,
                  ST_AEAD_AAD, sizeof ST_AEAD_AAD,
                  buf, sizeof buf, short_tag) != 0) return false;
    if (memcmp(buf, ST_AEAD_PT, sizeof buf) != 0) return false;

    return true;
}
