#include <stdbool.h>
#include <stdio.h>

#include "proto_zeroize.h"

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static void fill_secret(unsigned char *buf, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        buf[i] = 0xA5;
    }
}

static bool all_zero(const unsigned char *buf, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        if (buf[i] != 0) {
            return false;
        }
    }
    return true;
}

static void test_zeroize_wipes_filled_buffer(void) {
    unsigned char secret[64];

    fill_secret(secret, sizeof secret);
    CHECK(!all_zero(secret, sizeof secret));
    zeroize(secret, sizeof secret);
    CHECK(all_zero(secret, sizeof secret));
}

static void test_zeroize_handles_various_lengths(void) {
    unsigned char buf[33];
    size_t len;

    for (len = 1; len <= sizeof buf; len++) {
        fill_secret(buf, sizeof buf);
        zeroize(buf, len);
        CHECK(all_zero(buf, len));
        if (len < sizeof buf) {
            CHECK(buf[len] == 0xA5); /* bytes past len are untouched */
        }
    }
}

static void test_zeroize_null_and_zero_length_are_safe(void) {
    unsigned char buf[8];

    fill_secret(buf, sizeof buf);
    zeroize(NULL, 0);
    zeroize(NULL, 32);
    zeroize(buf, 0);
    CHECK(buf[0] == 0xA5);
    zeroize(buf, sizeof buf);
    CHECK(all_zero(buf, sizeof buf));
}

static void test_zeroize_repeated_is_safe(void) {
    unsigned char buf[16];

    fill_secret(buf, sizeof buf);
    zeroize(buf, sizeof buf);
    zeroize(buf, sizeof buf);
    CHECK(all_zero(buf, sizeof buf));

    fill_secret(buf, sizeof buf);
    zeroize(buf, sizeof buf);
    CHECK(all_zero(buf, sizeof buf));
}

static void test_tracker_init_and_count(void) {
    zeroize_tracker_t tracker;

    zeroize_tracker_init(&tracker);
    CHECK(zeroize_tracker_count(&tracker) == 0u);
    CHECK(zeroize_tracker_count(NULL) == 0u);
}

static void test_tracker_clear_all_zeroizes_multiple_buffers(void) {
    zeroize_tracker_t tracker;
    unsigned char a[16];
    unsigned char b[32];
    unsigned char c[8];

    zeroize_tracker_init(&tracker);
    CHECK(zeroize_tracker_register(&tracker, a, sizeof a));
    CHECK(zeroize_tracker_register(&tracker, b, sizeof b));
    CHECK(zeroize_tracker_register(&tracker, c, sizeof c));
    CHECK(zeroize_tracker_count(&tracker) == 3u);

    fill_secret(a, sizeof a);
    fill_secret(b, sizeof b);
    fill_secret(c, sizeof c);
    zeroize_tracker_clear_all(&tracker);

    CHECK(all_zero(a, sizeof a));
    CHECK(all_zero(b, sizeof b));
    CHECK(all_zero(c, sizeof c));
    CHECK(zeroize_tracker_count(&tracker) == 0u);
}

static void test_tracker_register_rejects_invalid_inputs(void) {
    zeroize_tracker_t tracker;
    unsigned char buf[8];

    zeroize_tracker_init(&tracker);
    CHECK(!zeroize_tracker_register(NULL, buf, sizeof buf));
    CHECK(!zeroize_tracker_register(&tracker, NULL, sizeof buf));
    CHECK(!zeroize_tracker_register(&tracker, buf, 0u));
    CHECK(zeroize_tracker_count(&tracker) == 0u);

    CHECK(zeroize_tracker_register(&tracker, buf, sizeof buf));
    CHECK(zeroize_tracker_count(&tracker) == 1u);
}

static void test_tracker_capacity_is_bounded(void) {
    zeroize_tracker_t tracker;
    unsigned char bufs[ZEROIZE_TRACKER_MAX][4];
    unsigned char extra[4];
    size_t i;

    zeroize_tracker_init(&tracker);
    for (i = 0; i < ZEROIZE_TRACKER_MAX; i++) {
        CHECK(zeroize_tracker_register(&tracker, bufs[i], sizeof bufs[i]));
    }
    CHECK(zeroize_tracker_count(&tracker) == (size_t)ZEROIZE_TRACKER_MAX);
    CHECK(!zeroize_tracker_register(&tracker, extra, sizeof extra));
    CHECK(zeroize_tracker_count(&tracker) == (size_t)ZEROIZE_TRACKER_MAX);

    fill_secret(extra, sizeof extra);
    zeroize_tracker_clear_all(&tracker);
    for (i = 0; i < ZEROIZE_TRACKER_MAX; i++) {
        CHECK(all_zero(bufs[i], sizeof bufs[i]));
    }
    CHECK(extra[0] == 0xA5); /* never registered, never wiped */
}

static void test_tracker_null_and_repeated_clear_all_are_safe(void) {
    zeroize_tracker_t tracker;
    unsigned char buf[8];

    zeroize_tracker_init(NULL);
    zeroize_tracker_clear_all(NULL);

    zeroize_tracker_init(&tracker);
    CHECK(zeroize_tracker_register(&tracker, buf, sizeof buf));
    fill_secret(buf, sizeof buf);
    zeroize_tracker_clear_all(&tracker);
    zeroize_tracker_clear_all(&tracker);
    CHECK(all_zero(buf, sizeof buf));
    CHECK(zeroize_tracker_count(&tracker) == 0u);

    fill_secret(buf, sizeof buf);
    CHECK(zeroize_tracker_register(&tracker, buf, sizeof buf));
    zeroize_tracker_clear_all(&tracker);
    CHECK(all_zero(buf, sizeof buf));
}

static void test_tracker_init_scrubs_stale_registrations(void) {
    zeroize_tracker_t tracker;
    unsigned char buf[8];

    zeroize_tracker_init(&tracker);
    CHECK(zeroize_tracker_register(&tracker, buf, sizeof buf));
    fill_secret(buf, sizeof buf);

    zeroize_tracker_init(&tracker);
    CHECK(zeroize_tracker_count(&tracker) == 0u);
    CHECK(buf[0] == 0xA5); /* stale entry forgotten, caller buffer untouched */
}

int main(void) {
    test_zeroize_wipes_filled_buffer();
    test_zeroize_handles_various_lengths();
    test_zeroize_null_and_zero_length_are_safe();
    test_zeroize_repeated_is_safe();
    test_tracker_init_and_count();
    test_tracker_clear_all_zeroizes_multiple_buffers();
    test_tracker_register_rejects_invalid_inputs();
    test_tracker_capacity_is_bounded();
    test_tracker_null_and_repeated_clear_all_are_safe();
    test_tracker_init_scrubs_stale_registrations();

    if (failures) {
        printf("zeroize: %d failure(s)\n", failures);
        return 1;
    }
    printf("zeroize: PASS\n");
    return 0;
}
