#include "proto_sfo.h"

/* The first 16 lexicographic permutations of {0,1,2,3}. The remaining 8 of
 * the 24 possible orders are deliberately unused and rejected on decode. */
static const uint8_t PERMS[SFO_PERM_COUNT][SFO_BURST_SIZE] = {
    {0,1,2,3}, {0,1,3,2}, {0,2,1,3}, {0,2,3,1},
    {0,3,1,2}, {0,3,2,1}, {1,0,2,3}, {1,0,3,2},
    {1,2,0,3}, {1,2,3,0}, {1,3,0,2}, {1,3,2,0},
    {2,0,1,3}, {2,0,3,1}, {2,1,0,3}, {2,1,3,0}
};

void sfo_nibble_to_perm(uint8_t nibble, uint8_t perm[SFO_BURST_SIZE]) {
    uint8_t i = (uint8_t)(nibble % SFO_PERM_COUNT);
    uint8_t j;
    for (j = 0; j < SFO_BURST_SIZE; j++) perm[j] = PERMS[i][j];
}

bool sfo_perm_to_nibble(const uint8_t order[SFO_BURST_SIZE], uint8_t *nibble) {
    uint8_t i, j;
    if (!order || !nibble) return false;
    for (i = 0; i < SFO_PERM_COUNT; i++) {
        bool match = true;
        for (j = 0; j < SFO_BURST_SIZE; j++) {
            if (order[j] != PERMS[i][j]) { match = false; break; }
        }
        if (match) { *nibble = i; return true; }
    }
    return false;
}

uint8_t sfo_crc8(uint8_t context, uint8_t nibble) {
    uint8_t crc = context;
    uint8_t byte = nibble;
    uint8_t i;
    for (i = 0; i < 8u; i++) {
        uint8_t bit = (uint8_t)((crc ^ byte) >> 7u);
        crc = (uint8_t)(crc << 1);
        if (bit) crc ^= 0x07u;
        byte = (uint8_t)(byte << 1);
    }
    return crc;
}

bool sfo_majority(const uint8_t nibbles[SFO_REPEATS], uint8_t *nibble) {
    uint8_t i, j;
    if (!nibbles || !nibble) return false;
    for (i = 0; i < SFO_REPEATS; i++) {
        uint8_t votes = 0;
        for (j = 0; j < SFO_REPEATS; j++) {
            if (nibbles[j] == nibbles[i]) votes++;
        }
        if (votes >= 2u) { *nibble = nibbles[i]; return true; }
    }
    return false;
}
