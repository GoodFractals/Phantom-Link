#ifndef PHANTOM_PROTO_HDR_H
#define PHANTOM_PROTO_HDR_H

/* Encrypted header envelope for the Bonnie & Clyde protocol (ticket 34).
 *
 * SDK-free: this header and proto_hdr.c compile into firmware and into the
 * host tests with exactly the same wire semantics.
 *
 * Frame header (18 bytes):
 *
 *   off  size  field          visibility   role
 *    0    2    magic 'B','C'   clear        AAD
 *    2    1    version         clear        AAD
 *    3    1    type            clear        AAD
 *    4    1    flags           clear        AAD
 *    5    1    payload_len     clear        AAD
 *    6    4    session_id      secret       encrypted
 *   10    4    seq             secret       encrypted
 *   14    4    ack             secret       encrypted
 *
 * hdr_seal copies the 6 clear bytes verbatim into out_clear; those bytes are
 * the AEAD associated data. The 12 secret bytes (session_id || seq || ack)
 * are encrypted with Speck128/128 in CTR mode into out_ct. The 8-byte tag is
 * CMAC-Speck128 truncated MSB-first over
 *
 *     AAD (6 clear bytes) || ciphertext (12 bytes)  = 18 on-wire bytes.
 *
 * Verify-before-decrypt: hdr_open recomputes that tag and compares it in
 * constant time; on any mismatch it returns false and leaves `recovered`
 * completely untouched.
 *
 * Nonce (12 bytes, PC_NONCE_BYTES):
 *
 *     byte 0      0x02   domain byte (0x01 is the payload-encryption domain)
 *     bytes 1..4  seq, 4 bytes little-endian, taken from secret bytes 4..7
 *     bytes 5..11 seven zero bytes
 *
 * The seq is itself encrypted, so it cannot be read back from the ciphertext
 * before decryption. hdr_open therefore takes the 4 seq bytes explicitly;
 * the caller supplies the sequence number from its session/replay state.
 * hdr_open verifies the tag first, decrypts into a temporary, and checks that
 * the decrypted seq matches the supplied one. A wrong seq, key, AAD or tag is
 * rejected, and every rejection path leaves `recovered` untouched.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "proto_crypto.h"

#define HDR_CLEAR_BYTES 6u
#define HDR_SECRET_BYTES 12u
#define HDR_HEADER_BYTES 18u
#define HDR_SEQ_BYTES 4u
#define HDR_TAG_BYTES PC_TAG_BYTES
#define HDR_NONCE_BYTES PC_NONCE_BYTES

/* Seal one header. `clear` is the 6-byte clear/AAD prefix; `secret` is the
 * 12-byte session_id || seq || ack block. */
void hdr_seal(const uint8_t key[SPECK128_KEY_BYTES],
              const uint8_t clear[HDR_CLEAR_BYTES],
              const uint8_t secret[HDR_SECRET_BYTES],
              uint8_t out_clear[HDR_CLEAR_BYTES],
              uint8_t out_ct[HDR_SECRET_BYTES],
              uint8_t tag[HDR_TAG_BYTES]);

/* Authenticate then decrypt one header. `seq` is the 4 little-endian sequence
 * bytes from the secret field (secret[4..7]) that seed the nonce. Returns
 * true only on a valid tag; on failure `recovered` is not written. */
bool hdr_open(const uint8_t key[SPECK128_KEY_BYTES],
              const uint8_t clear[HDR_CLEAR_BYTES],
              const uint8_t ct[HDR_SECRET_BYTES],
              const uint8_t seq[HDR_SEQ_BYTES],
              const uint8_t tag[HDR_TAG_BYTES],
              uint8_t recovered[HDR_SECRET_BYTES]);

#endif /* PHANTOM_PROTO_HDR_H */
