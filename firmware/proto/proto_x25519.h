#ifndef PHANTOM_X25519_H
#define PHANTOM_X25519_H

/* X25519 ephemeral key exchange for Phantom Link forward secrecy (ticket 31).
 *
 * Firmware-only: wraps mbedTLS ECDH on Curve25519. The host-side KDF that
 * consumes the shared secret is proto_fs.c (tested with RFC 7748 vectors).
 *
 * Wire repr: a public key is the 32-byte little-endian u-coordinate.
 * The shared secret is the 32-byte raw X25519 output (before KDF).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define X25519_KEY_BYTES 32u

/* Generate an ephemeral keypair. Writes our 32-byte public key to pub_out and
 * keeps the private scalar in ctx (opaque, sized here to avoid mbedTLS headers
 * in callers). Returns false on failure. */
typedef struct {
    uint8_t opaque[256]; /* enough for mbedtls_ecp_group + mbedtls_mpi */
} x25519_ctx_t;

bool x25519_gen_keypair(x25519_ctx_t *ctx, uint8_t pub_out[X25519_KEY_BYTES]);

/* Compute the shared secret from our private ctx and the peer's public key.
 * Writes 32 raw bytes to shared_out. Returns false on failure/invalid point. */
bool x25519_shared_secret(x25519_ctx_t *ctx,
                          const uint8_t peer_pub[X25519_KEY_BYTES],
                          uint8_t shared_out[X25519_KEY_BYTES]);

/* Best-effort wipe of the private context (call on teardown). */
void x25519_clear(x25519_ctx_t *ctx);

/* Self-test: two keypairs, both sides derive the same secret; returns true on
 * success. Used to gate LINK UP. */
bool x25519_selftest(void);

#endif
