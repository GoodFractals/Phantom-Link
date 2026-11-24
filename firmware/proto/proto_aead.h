#ifndef PHANTOM_AEAD_H
#define PHANTOM_AEAD_H

/* ChaCha20-Poly1305 AEAD suite (RFC 8439), with an 8-byte truncated tag.
 *
 * Host-only, standalone module for ticket 32.  Deliberately SDK-free and
 * decoupled from proto_crypto.h: the suite_ops shape mirrors pc_suite_ops_t
 * conceptually, but the types are defined locally so this module compiles and
 * tests on its own.  C11, no dynamic allocation, constant-time tag check.
 *
 * Wire semantics match the existing suite: AAD is the 18-byte frame header,
 * seal encrypts `buf` in place and writes an 8-byte tag, open verifies the tag
 * before decrypting.  Encryption and the Poly1305 one-time key both use the
 * same 32-byte key; block counter 0 derives the Poly1305 key and counter 1+
 * produces the keystream (encrypt-then-MAC, per RFC 8439 section 2.8).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHACHA20_KEY_BYTES 32u
#define CHACHA20_NONCE_BYTES 12u
#define CHACHA20_BLOCK_BYTES 64u

#define POLY1305_KEY_BYTES 32u
#define POLY1305_TAG_BYTES 16u

#define AEAD_KEY_BYTES CHACHA20_KEY_BYTES
#define AEAD_NONCE_BYTES CHACHA20_NONCE_BYTES
#define AEAD_TAG_BYTES 8u
#define AEAD_AAD_BYTES 18u

/* ChaCha20 block function. `counter` is the 32-bit block counter. */
void chacha20_block(const uint8_t key[CHACHA20_KEY_BYTES],
                    uint32_t counter,
                    const uint8_t nonce[CHACHA20_NONCE_BYTES],
                    uint8_t out[CHACHA20_BLOCK_BYTES]);

/* Poly1305 MAC. `msg` may be NULL when `len` is 0. */
void poly1305(const uint8_t key[POLY1305_KEY_BYTES],
              const uint8_t *msg, size_t len,
              uint8_t tag[POLY1305_TAG_BYTES]);

/* Cipher-agnostic suite shape (mirrors pc_suite_ops_t, defined locally). */
typedef struct {
    void (*seal)(const uint8_t key[AEAD_KEY_BYTES],
                 const uint8_t nonce[AEAD_NONCE_BYTES],
                 const uint8_t *aad, size_t aad_len,
                 uint8_t *buf, size_t len,
                 uint8_t tag[AEAD_TAG_BYTES]);
    int (*open)(const uint8_t key[AEAD_KEY_BYTES],
                const uint8_t nonce[AEAD_NONCE_BYTES],
                const uint8_t *aad, size_t aad_len,
                uint8_t *buf, size_t len,
                const uint8_t tag[AEAD_TAG_BYTES]);
} aead_suite_ops_t;

extern const aead_suite_ops_t aead_suite_chacha20_poly1305;

void aead_seal(const uint8_t key[AEAD_KEY_BYTES],
               const uint8_t nonce[AEAD_NONCE_BYTES],
               const uint8_t *aad, size_t aad_len,
               uint8_t *buf, size_t len,
               uint8_t tag[AEAD_TAG_BYTES]);

/* Returns 0 on a valid frame, -1 on tag mismatch (no decryption performed). */
int aead_open(const uint8_t key[AEAD_KEY_BYTES],
              const uint8_t nonce[AEAD_NONCE_BYTES],
              const uint8_t *aad, size_t aad_len,
              uint8_t *buf, size_t len,
              const uint8_t tag[AEAD_TAG_BYTES]);

/* RFC 8439 KAT runner; returns true when every embedded vector passes. */
bool aead_self_test(void);

#endif
