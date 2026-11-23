#ifndef PHANTOM_SFO_H
#define PHANTOM_SFO_H

/* Application-layer SFO burst channel. Carries one nibble per 4-datagram
 * arrival permutation. Telemetry only; never gates delivery (ADR 0002). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SFO_BURST_SIZE 4u
#define SFO_REPEATS 3u
#define SFO_PERM_COUNT 16u

/* Maps a nibble to the transmission indices whose arrival order encodes it. */
void sfo_nibble_to_perm(uint8_t nibble, uint8_t perm[SFO_BURST_SIZE]);

/* Returns the nibble encoded by an arrival order, or false for unused codes. */
bool sfo_perm_to_nibble(const uint8_t order[SFO_BURST_SIZE], uint8_t *nibble);

/* CRC-8 (poly 0x07, init 0) over the nibble and a context byte. */
uint8_t sfo_crc8(uint8_t context, uint8_t nibble);

/* Majority decode of SFO_REPEATS candidate nibbles; false if no majority. */
bool sfo_majority(const uint8_t nibbles[SFO_REPEATS], uint8_t *nibble);

#endif
