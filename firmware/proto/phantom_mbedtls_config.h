/* Minimal mbedTLS config for Phantom Link on RP2350.
 * We only need ECDH (Curve25519) + bignum + SHA-256. Disable platform entropy
 * (no Unix/Windows entropy source on the Pico); our RNG callback supplies
 * randomness directly to mbedTLS. */
#ifndef PHANTOM_MBEDTLS_CONFIG_H
#define PHANTOM_MBEDTLS_CONFIG_H

#include "mbedtls/mbedtls_config.h"

/* Applied after the stock config so they override its defaults. */
#undef MBEDTLS_HAVE_TIME
#undef MBEDTLS_HAVE_TIME_DATE
#undef MBEDTLS_PLATFORM_MS_TIME_ALT
#undef MBEDTLS_TIMING_C
#define MBEDTLS_NO_PLATFORM_ENTROPY

#endif
