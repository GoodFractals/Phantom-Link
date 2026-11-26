#include "proto_x25519.h"

/* Firmware-only X25519 wrapper over mbedTLS 3.x ECDH on Curve25519.
 * Host tests exercise the KDF side (proto_fs.c) with RFC 7748 vectors; this
 * module supplies the real ephemeral keypair and shared secret on the device. */

#include <string.h>

#include "mbedtls/ecdh.h"
#include "mbedtls/ecp.h"
#include "mbedtls/bignum.h"
#include "pico/rand.h"

/* Private state stored in the caller's opaque buffer. */
typedef struct {
    mbedtls_ecp_group grp;
    mbedtls_mpi d;      /* our private scalar */
    bool ready;
} x25519_priv_t;

_Static_assert(sizeof(x25519_priv_t) <= sizeof(x25519_ctx_t),
               "x25519_ctx_t opaque buffer too small");

static int x25519_rng(void *arg, unsigned char *out, size_t len) {
    (void)arg;
    size_t i = 0;
    while (i < len) {
        uint32_t r = get_rand_32();
        size_t n = len - i < 4u ? len - i : 4u;
        memcpy(out + i, &r, n);
        i += n;
    }
    return 0;
}

bool x25519_gen_keypair(x25519_ctx_t *ctx,
                        uint8_t pub_out[X25519_KEY_BYTES]) {
    x25519_priv_t *p = (x25519_priv_t *)ctx;
    mbedtls_ecp_point Q;
    int rc;

    if (!ctx || !pub_out) return false;
    memset(ctx, 0, sizeof *ctx);
    mbedtls_ecp_group_init(&p->grp);
    mbedtls_mpi_init(&p->d);
    mbedtls_ecp_point_init(&Q);

    rc = mbedtls_ecp_group_load(&p->grp, MBEDTLS_ECP_DP_CURVE25519);
    if (rc == 0)
        rc = mbedtls_ecdh_gen_public(&p->grp, &p->d, &Q,
                                     x25519_rng, NULL);
    /* X25519 public key = u-coordinate, little-endian on the wire. */
    if (rc == 0)
        rc = mbedtls_mpi_write_binary_le(&Q.MBEDTLS_PRIVATE(X),
                                         pub_out, X25519_KEY_BYTES);

    mbedtls_ecp_point_free(&Q);
    if (rc == 0) {
        p->ready = true;
    } else {
        mbedtls_mpi_free(&p->d);
        mbedtls_ecp_group_free(&p->grp);
    }
    return p->ready;
}

bool x25519_shared_secret(x25519_ctx_t *ctx,
                          const uint8_t peer_pub[X25519_KEY_BYTES],
                          uint8_t shared_out[X25519_KEY_BYTES]) {
    x25519_priv_t *p = (x25519_priv_t *)ctx;
    mbedtls_ecp_point Q;
    mbedtls_mpi z;
    int rc;

    if (!p || !p->ready || !peer_pub || !shared_out) return false;
    mbedtls_ecp_point_init(&Q);
    mbedtls_mpi_init(&z);

    /* Load the peer's u-coordinate from the little-endian wire repr. */
    rc = mbedtls_mpi_read_binary_le(&Q.MBEDTLS_PRIVATE(X),
                                    peer_pub, X25519_KEY_BYTES);
    if (rc == 0)
        rc = mbedtls_mpi_lset(&Q.MBEDTLS_PRIVATE(Z), 1);
    if (rc == 0)
        rc = mbedtls_ecdh_compute_shared(&p->grp, &z, &Q, &p->d,
                                         x25519_rng, NULL);
    if (rc == 0)
        rc = mbedtls_mpi_write_binary_le(&z, shared_out, X25519_KEY_BYTES);

    mbedtls_mpi_free(&z);
    mbedtls_ecp_point_free(&Q);
    return rc == 0;
}

void x25519_clear(x25519_ctx_t *ctx) {
    x25519_priv_t *p = (x25519_priv_t *)ctx;
    if (!ctx) return;
    if (p->ready) {
        mbedtls_mpi_free(&p->d);
        mbedtls_ecp_group_free(&p->grp);
    }
    memset(ctx, 0, sizeof *ctx);
}

bool x25519_selftest(void) {
    x25519_ctx_t a, b;
    uint8_t pub_a[X25519_KEY_BYTES], pub_b[X25519_KEY_BYTES];
    uint8_t s_a[X25519_KEY_BYTES], s_b[X25519_KEY_BYTES];
    bool ok = x25519_gen_keypair(&a, pub_a) &&
              x25519_gen_keypair(&b, pub_b) &&
              x25519_shared_secret(&a, pub_b, s_a) &&
              x25519_shared_secret(&b, pub_a, s_b) &&
              memcmp(s_a, s_b, X25519_KEY_BYTES) == 0;
    x25519_clear(&a);
    x25519_clear(&b);
    return ok;
}
