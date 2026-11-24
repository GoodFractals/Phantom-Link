#include <stdio.h>
#include <string.h>

#include "proto_fs.h"

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

/* RFC 7748 section 6.1: Alice/Bob shared secret. */
static const uint8_t SHARED[32] = {
    0x4a,0x5d,0x9d,0x5b,0xa4,0xce,0x2d,0xe1,0x72,0x8e,0x3b,0xf4,0x80,0x35,0x0f,0x25,
    0xe0,0x7e,0x21,0xc9,0x47,0xd1,0x9e,0x33,0x76,0xf0,0x9b,0x3c,0x1e,0x16,0x17,0x42
};
static const uint8_t PSK[32] = {
    0x54,0x68,0x69,0x73,0x49,0x73,0x41,0x50,0x6c,0x61,0x63,0x65,0x68,0x6f,0x6c,0x64,
    0x42,0x75,0x69,0x6c,0x64,0x42,0x79,0x20,0x43,0x68,0x61,0x6e,0x67,0x65,0x21,0x21
};
static const uint8_t SALT[] = "BONNIE-CLYDE-v1";

static void test_key_mixing(void) {
    uint8_t a[32], b[32], a2[32], b2[32];

    fs_derive_keys(PSK, SHARED, 0x11223344u, SALT, sizeof SALT - 1u, a, b);
    fs_derive_keys(PSK, SHARED, 0x11223344u, SALT, sizeof SALT - 1u, a2, b);
    CHECK(memcmp(a, a2, sizeof a) == 0);
    CHECK(memcmp(a, b, sizeof a) != 0);
    fs_derive_keys(PSK, SHARED, 0x11223345u, SALT, sizeof SALT - 1u, a2, b2);
    CHECK(memcmp(a, a2, sizeof a) != 0);
}

static void test_shared_secret_binding(void) {
    uint8_t a[32], b[32];
    uint8_t other[32];

    memcpy(other, SHARED, sizeof other);
    other[0] ^= 1u;
    fs_derive_keys(PSK, SHARED, 0x11223344u, SALT, sizeof SALT - 1u, a, b);
    fs_derive_keys(PSK, other, 0x11223344u, SALT, sizeof SALT - 1u, a, b);
    /* a now holds the key from the modified ECDH secret. */
    CHECK(memcmp(a, b, sizeof a) != 0);
}

static void test_epoch_monotonic(void) {
    fs_session_t s;

    fs_session_init(&s);
    CHECK(!fs_session_accept_epoch(&s, 1u));
    CHECK(fs_session_adopt(&s, 10u));
    CHECK(fs_session_accept_epoch(&s, 10u));
    CHECK(!fs_session_accept_epoch(&s, 9u));
    CHECK(!fs_session_adopt(&s, 10u));
    CHECK(fs_session_adopt(&s, 11u));
    CHECK(fs_session_accept_epoch(&s, 11u));
}

int main(void) {
    test_key_mixing();
    test_shared_secret_binding();
    test_epoch_monotonic();

    if (failures) {
        printf("fs: %d failure(s)\n", failures);
        return 1;
    }
    printf("fs: PASS\n");
    return 0;
}
