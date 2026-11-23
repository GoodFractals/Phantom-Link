#ifndef PHANTOM_LPI_H
#define PHANTOM_LPI_H

/* Application-layer LPI helpers. No Pico SDK dependency. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LPI_WIRE_PAYLOAD 64u

/* Formats a plaintext payload into a constant-length wire payload:
 *   [0] real length, [1..real] payload, [real+1..63] random padding.
 * rng is called once per padding word; pass a hardware RNG on device. */
void lpi_pad(const uint8_t *src, size_t len,
             uint8_t out[LPI_WIRE_PAYLOAD], uint32_t (*rng)(void));

/* Recovers the real payload. Returns false if the length byte is invalid. */
bool lpi_unpad(const uint8_t in[LPI_WIRE_PAYLOAD], size_t in_len,
               uint8_t *out, size_t *out_len);

/* Returns base + [0, jitter). Deterministic given rng. */
uint32_t lpi_decoy_delay(uint32_t base, uint32_t jitter,
                         uint32_t (*rng)(void));

#endif
