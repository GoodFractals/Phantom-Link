#include "proto_hdr.h"

#include <string.h>

/* proto_crypto.h exposes the block primitive but keeps its CMAC adapter
 * private, so provide the same adapter here for the header tag. */
static void hdr_speck_block(const void *key_ctx,
                            const uint8_t in[PC_CMAC_BLOCK_BYTES],
                            uint8_t out[PC_CMAC_BLOCK_BYTES]) {
    speck128_encrypt((const speck128_key_t *)key_ctx, in, out);
}

/* nonce = 0x02 || seq(4 LE) || 0^7 (see proto_hdr.h). */
static void hdr_make_nonce(const uint8_t seq[HDR_SEQ_BYTES],
                           uint8_t nonce[HDR_NONCE_BYTES]) {
    nonce[0] = 0x02u;
    memcpy(nonce + 1u, seq, HDR_SEQ_BYTES);
    memset(nonce + 1u + HDR_SEQ_BYTES, 0,
           HDR_NONCE_BYTES - 1u - HDR_SEQ_BYTES);
}

/* Tag covers the full 18-byte on-wire header: clear AAD then ciphertext. */
static void hdr_tag(const uint8_t key[SPECK128_KEY_BYTES],
                    const uint8_t clear[HDR_CLEAR_BYTES],
                    const uint8_t ct[HDR_SECRET_BYTES],
                    uint8_t tag[HDR_TAG_BYTES]) {
    uint8_t wire[HDR_HEADER_BYTES];
    speck128_key_t sk;

    memcpy(wire, clear, HDR_CLEAR_BYTES);
    memcpy(wire + HDR_CLEAR_BYTES, ct, HDR_SECRET_BYTES);

    speck128_set_key(&sk, key);
    pc_cmac_trunc(hdr_speck_block, &sk, wire, sizeof wire, tag);
}

void hdr_seal(const uint8_t key[SPECK128_KEY_BYTES],
              const uint8_t clear[HDR_CLEAR_BYTES],
              const uint8_t secret[HDR_SECRET_BYTES],
              uint8_t out_clear[HDR_CLEAR_BYTES],
              uint8_t out_ct[HDR_SECRET_BYTES],
              uint8_t tag[HDR_TAG_BYTES]) {
    uint8_t nonce[HDR_NONCE_BYTES];

    memcpy(out_clear, clear, HDR_CLEAR_BYTES);

    hdr_make_nonce(secret + 4u, nonce);
    memcpy(out_ct, secret, HDR_SECRET_BYTES);
    pc_ctr_xcrypt(key, nonce, out_ct, HDR_SECRET_BYTES);

    hdr_tag(key, out_clear, out_ct, tag);
}

bool hdr_open(const uint8_t key[SPECK128_KEY_BYTES],
              const uint8_t clear[HDR_CLEAR_BYTES],
              const uint8_t ct[HDR_SECRET_BYTES],
              const uint8_t seq[HDR_SEQ_BYTES],
              const uint8_t tag[HDR_TAG_BYTES],
              uint8_t recovered[HDR_SECRET_BYTES]) {
    uint8_t expected[HDR_TAG_BYTES];
    uint8_t nonce[HDR_NONCE_BYTES];
    uint8_t plain[HDR_SECRET_BYTES];
    unsigned diff = 0u;
    unsigned i;

    hdr_tag(key, clear, ct, expected);
    for (i = 0u; i < HDR_TAG_BYTES; i++) {
        diff |= (unsigned)(expected[i] ^ tag[i]);
    }
    if (diff != 0u) return false; /* verify before decrypt, no writes */

    hdr_make_nonce(seq, nonce);
    memcpy(plain, ct, HDR_SECRET_BYTES);
    pc_ctr_xcrypt(key, nonce, plain, HDR_SECRET_BYTES);

    /* Bind the caller's seq to the decrypted header. Decrypt into `plain`
     * first so every rejection path leaves `recovered` untouched. */
    diff = 0u;
    for (i = 0u; i < HDR_SEQ_BYTES; i++) {
        diff |= (unsigned)(plain[4u + i] ^ seq[i]);
    }
    if (diff != 0u) return false;

    memcpy(recovered, plain, HDR_SECRET_BYTES);
    return true;
}
