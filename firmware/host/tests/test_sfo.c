#include <stdio.h>

#include "proto_sfo.h"

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static void test_all_nibbles(void) {
    uint8_t nibble;
    for (nibble = 0; nibble < SFO_PERM_COUNT; nibble++) {
        uint8_t perm[SFO_BURST_SIZE], decoded = 0xFFu;
        sfo_nibble_to_perm(nibble, perm);
        CHECK(sfo_perm_to_nibble(perm, &decoded));
        CHECK(decoded == nibble);
    }
}

static void test_unused_permutations(void) {
    static const uint8_t unused[][SFO_BURST_SIZE] = {
        {2,3,0,1}, {2,3,1,0}, {3,0,1,2}, {3,0,2,1},
        {3,1,0,2}, {3,1,2,0}, {3,2,0,1}, {3,2,1,0}
    };
    size_t i;
    for (i = 0; i < sizeof unused / sizeof unused[0]; i++) {
        uint8_t decoded = 0xFFu;
        CHECK(!sfo_perm_to_nibble(unused[i], &decoded));
    }
}

static void test_duplicate_index(void) {
    uint8_t decoded = 0xFFu;
    static const uint8_t dup1[4] = {0,0,1,2};
    static const uint8_t dup2[4] = {3,1,3,0};
    CHECK(!sfo_perm_to_nibble(dup1, &decoded));
    CHECK(!sfo_perm_to_nibble(dup2, &decoded));
}

static void test_crc8(void) {
    uint8_t a = sfo_crc8(0x11u, 0x05u);
    CHECK(a == sfo_crc8(0x11u, 0x05u));
    CHECK(a != sfo_crc8(0x11u, 0x06u));
    CHECK(a != sfo_crc8(0x12u, 0x05u));
}

static void test_majority(void) {
    uint8_t good[SFO_REPEATS] = { 7u, 7u, 9u };
    uint8_t no_majority[SFO_REPEATS] = { 1u, 2u, 3u };
    uint8_t out = 0xFFu;

    CHECK(sfo_majority(good, &out));
    CHECK(out == 7u);
    CHECK(!sfo_majority(no_majority, &out));
}

int main(void) {
    test_all_nibbles();
    test_unused_permutations();
    test_duplicate_index();
    test_crc8();
    test_majority();

    if (failures) {
        printf("sfo: %d failure(s)\n", failures);
        return 1;
    }
    printf("sfo: PASS\n");
    return 0;
}
