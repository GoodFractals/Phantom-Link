#ifndef PHANTOM_CRYPTO_H
#define PHANTOM_CRYPTO_H

/* Cipher-agnostic crypto primitives for Phantom Link.
 *
 * No Pico SDK headers here: the same source compiles into host tests and
 * firmware. Wire format and protocol timing constants are unchanged.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SPECK128_KEY_BYTES 16u
#define SPECK128_BLOCK_BYTES 16u
#define SPECK128_ROUNDS 32u

typedef struct {
    uint64_t rk[SPECK128_ROUNDS];
} speck128_key_t;

/* Loads a 128-bit key as two little-endian 64-bit words (K[0], K[1]). */
void speck128_set_key(speck128_key_t *ctx, const uint8_t key[SPECK128_KEY_BYTES]);

/* Encrypts one 128-bit block. Input and output may not overlap partially. */
void speck128_encrypt(const speck128_key_t *ctx,
                      const uint8_t in[SPECK128_BLOCK_BYTES],
                      uint8_t out[SPECK128_BLOCK_BYTES]);

/* Generic CMAC over any 128-bit block primitive. The block function receives
 * its own key context and one canonical 16-byte block. */
typedef void (*pc_block_fn)(const void *key_ctx,
                            const uint8_t in[16],
                            uint8_t out[16]);

#define PC_CMAC_BLOCK_BYTES 16u
#define PC_TAG_BYTES 8u

void pc_cmac_subkeys(pc_block_fn block, const void *key_ctx,
                     uint8_t k1[PC_CMAC_BLOCK_BYTES],
                     uint8_t k2[PC_CMAC_BLOCK_BYTES]);

void pc_cmac(pc_block_fn block, const void *key_ctx,
             const uint8_t *msg, size_t len,
             uint8_t tag[PC_CMAC_BLOCK_BYTES]);

/* MSB-first truncation to fill the 8-byte frame auth field. */
void pc_cmac_trunc(pc_block_fn block, const void *key_ctx,
                   const uint8_t *msg, size_t len,
                   uint8_t tag[PC_TAG_BYTES]);

/* Counter-mode KDF with CMAC as PRF (no hash primitive in this codebase).
 * Two 128-bit halves of the master key each produce one 16-byte output block;
 * the label domain-separates send and receive directions. */
#define PC_MASTER_KEY_BYTES 32u
#define PC_DERIVED_KEY_BYTES 32u

void pc_kdf(const uint8_t master[PC_MASTER_KEY_BYTES],
            const uint8_t *salt, size_t salt_len,
            uint8_t label,
            uint8_t out[PC_DERIVED_KEY_BYTES]);

/* CTR mode over Speck128/128. Counter is the low 32 bits, big-endian. */
#define PC_NONCE_BYTES 12u

void pc_ctr_xcrypt(const uint8_t key[SPECK128_KEY_BYTES],
                   const uint8_t nonce[PC_NONCE_BYTES],
                   uint8_t *buf, size_t len);

/* Cipher-agnostic suite: the session layer calls seal/open only. AAD is the
 * 18-byte frame header; seal writes ciphertext in place and an 8-byte tag. */
typedef struct {
    void (*seal)(const uint8_t key[SPECK128_KEY_BYTES],
                 const uint8_t nonce[PC_NONCE_BYTES],
                 const uint8_t *aad, size_t aad_len,
                 uint8_t *buf, size_t len,
                 uint8_t tag[PC_TAG_BYTES]);
    int (*open)(const uint8_t key[SPECK128_KEY_BYTES],
                const uint8_t nonce[PC_NONCE_BYTES],
                const uint8_t *aad, size_t aad_len,
                uint8_t *buf, size_t len,
                const uint8_t tag[PC_TAG_BYTES]);
} pc_suite_ops_t;

extern const pc_suite_ops_t pc_suite_speck_ctr_cmac;
/* Reserved AEAD slot; today it is a loud stub, never selected silently. */
extern const pc_suite_ops_t pc_suite_aead;

#define PC_CONFIRM_NONCE_BYTES 8u
#define PC_CONFIRM_TAG_BYTES 8u

void pc_confirm_tag(const uint8_t key[SPECK128_KEY_BYTES],
                    uint8_t label,
                    uint32_t session_id,
                    uint32_t seq,
                    const uint8_t nonce[PC_CONFIRM_NONCE_BYTES],
                    uint8_t tag[PC_CONFIRM_TAG_BYTES]);

bool pc_confirm_check(const uint8_t key[SPECK128_KEY_BYTES],
                      uint8_t label,
                      uint32_t session_id,
                      uint32_t seq,
                      const uint8_t nonce[PC_CONFIRM_NONCE_BYTES],
                      const uint8_t tag[PC_CONFIRM_TAG_BYTES]);

/* Runs the known-answer checks used at boot; LINK UP is gated on this. */
bool proto_crypto_self_test(void);

#endif
