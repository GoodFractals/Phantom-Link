#include "proto_lpi.h"

#include <string.h>

void lpi_pad(const uint8_t *src, size_t len,
             uint8_t out[LPI_WIRE_PAYLOAD], uint32_t (*rng)(void)) {
    size_t i;

    if (len > LPI_WIRE_PAYLOAD - 1u) len = LPI_WIRE_PAYLOAD - 1u;
    out[0] = (uint8_t)len;
    if (len) memcpy(out + 1u, src, len);

    for (i = len + 1u; i < LPI_WIRE_PAYLOAD;) {
        uint32_t word = rng ? rng() : 0u;
        unsigned j;
        for (j = 0; j < 4u && i < LPI_WIRE_PAYLOAD; j++, i++) {
            out[i] = (uint8_t)(word >> (8u * j));
        }
    }
}

bool lpi_unpad(const uint8_t in[LPI_WIRE_PAYLOAD], size_t in_len,
               uint8_t *out, size_t *out_len) {
    size_t real = in[0];

    if (in_len != LPI_WIRE_PAYLOAD || real > LPI_WIRE_PAYLOAD - 1u) return false;
    if (real) memcpy(out, in + 1u, real);
    *out_len = real;
    return true;
}

uint32_t lpi_decoy_delay(uint32_t base, uint32_t jitter,
                         uint32_t (*rng)(void)) {
    uint32_t offset = rng ? (rng() % (jitter ? jitter : 1u)) : 0u;
    return base + offset;
}
