#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "proto_reconnect.h"

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static void test_initial_state(void) {
    reconnect_t rc;

    reconnect_init(&rc);
    CHECK(rc.state == RECONNECT_STATE_DOWN);
    CHECK(rc.epoch == 0);
    CHECK(rc.backoff_ms == 0);
    CHECK(rc.next_attempt_ms == 0);
    CHECK(rc.attempts == 0);
    CHECK(rc.replacements == 0);
    CHECK(rc.stale_rejected == 0);
    CHECK(!rc.session_active);
}

static void test_down_to_connecting(void) {
    reconnect_t rc;
    reconnect_result_t r;

    reconnect_init(&rc);

    r = reconnect_tick(&rc, 1000, false);
    CHECK(r.state == RECONNECT_STATE_DOWN);
    CHECK(r.action == RECONNECT_ACTION_NONE);

    r = reconnect_tick(&rc, 1000, true);
    CHECK(r.state == RECONNECT_STATE_CONNECTING);
    CHECK(r.action == RECONNECT_ACTION_START);
    CHECK(rc.backoff_ms == RECONNECT_BACKOFF_MIN_MS);
    CHECK(rc.attempts == 1);

    r = reconnect_tick(&rc, 1001, true);
    CHECK(r.action == RECONNECT_ACTION_NONE);
    CHECK(rc.attempts == 1);
}

static void test_bounded_backoff(void) {
    static const uint32_t waits[] = { 500u, 1000u, 2000u, 4000u,
                                      5000u, 5000u, 5000u };
    static const uint32_t expected[] = { 1000u, 2000u, 4000u, 5000u,
                                         5000u, 5000u, 5000u };
    reconnect_t rc;
    reconnect_result_t r;
    uint32_t now = 0;
    size_t i;

    reconnect_init(&rc);
    (void)reconnect_adopt_epoch(&rc, 0, 1, true);
    r = reconnect_tick(&rc, 0, false);
    CHECK(r.state == RECONNECT_STATE_RECONNECTING);
    CHECK(r.action == RECONNECT_ACTION_LINK_DOWN);
    CHECK(r.backoff_ms == RECONNECT_BACKOFF_MIN_MS);

    for (i = 0; i < sizeof waits / sizeof waits[0]; i++) {
        now += waits[i];
        r = reconnect_tick(&rc, now, false);
        CHECK(r.state == RECONNECT_STATE_RECONNECTING);
        CHECK(r.action == RECONNECT_ACTION_RETRY);
        CHECK(rc.backoff_ms == expected[i]);
        CHECK(r.backoff_ms == expected[i]);
        CHECK(r.backoff_ms >= RECONNECT_BACKOFF_MIN_MS);
        CHECK(r.backoff_ms <= RECONNECT_BACKOFF_MAX_MS);
    }
    CHECK(rc.attempts == (uint32_t)(sizeof waits / sizeof waits[0]));
}

static void test_link_loss_enters_reconnecting(void) {
    reconnect_t rc;
    reconnect_result_t r;

    reconnect_init(&rc);
    r = reconnect_adopt_epoch(&rc, 100, 42, true);
    CHECK(r.state == RECONNECT_STATE_UP);
    CHECK(r.action == RECONNECT_ACTION_LINK_UP);
    r = reconnect_tick(&rc, 100, true);
    CHECK(r.state == RECONNECT_STATE_UP);
    CHECK(r.action == RECONNECT_ACTION_NONE);

    r = reconnect_tick(&rc, 200, false);
    CHECK(r.state == RECONNECT_STATE_RECONNECTING);
    CHECK(r.action == RECONNECT_ACTION_LINK_DOWN);
    CHECK(!rc.session_active);
    CHECK(rc.backoff_ms == RECONNECT_BACKOFF_MIN_MS);

    r = reconnect_tick(&rc, 201, true);
    CHECK(r.action == RECONNECT_ACTION_NONE);
    CHECK(r.state == RECONNECT_STATE_RECONNECTING);
}

static void test_ap_restart_relinks_under_ten_seconds(void) {
    reconnect_t rc;
    reconnect_result_t r;
    uint32_t now = 1000;
    uint32_t loss_ms;
    uint32_t relink_ms;

    reconnect_init(&rc);
    (void)reconnect_adopt_epoch(&rc, now, 10, true);
    CHECK(reconnect_tick(&rc, now, true).state == RECONNECT_STATE_UP);

    now += 1;
    loss_ms = now;
    r = reconnect_tick(&rc, now, false);
    CHECK(r.state == RECONNECT_STATE_RECONNECTING);

    /* AP stays away for 3 s, then the STA's Wi-Fi link returns. */
    now += 3000;
    relink_ms = now;
    while ((now - relink_ms) < RECONNECT_BACKOFF_MAX_MS + 100u) {
        r = reconnect_tick(&rc, now, true);
        if (r.action == RECONNECT_ACTION_RETRY) {
            (void)reconnect_adopt_epoch(&rc, now + 10u, 11u, true);
            now += 10u;
            break;
        }
        now += 50u;
    }

    CHECK(rc.state == RECONNECT_STATE_UP);
    CHECK(rc.epoch == 11);
    CHECK((now - loss_ms) < 10000u);
}

static void test_sta_restart_replaces_session(void) {
    reconnect_t rc;
    reconnect_result_t r;

    reconnect_init(&rc);
    (void)reconnect_adopt_epoch(&rc, 100, 7, true);
    CHECK(reconnect_tick(&rc, 100, true).state == RECONNECT_STATE_UP);

    r = reconnect_adopt_epoch(&rc, 200, 8, true);
    CHECK(r.action == RECONNECT_ACTION_LINK_UP);
    CHECK(r.link_up);
    CHECK(r.session_replaced);
    CHECK(r.epoch == 8);
    CHECK(rc.replacements == 1);
    CHECK(rc.session_active);

    /* No duplicate LINK UP on the following tick. */
    r = reconnect_tick(&rc, 201, true);
    CHECK(r.action == RECONNECT_ACTION_NONE);
    CHECK(!r.link_up);
    CHECK(rc.replacements == 1);

    /* A replacement staged while the link is down is reported as REPLACE,
     * never as a second LINK UP. */
    (void)reconnect_tick(&rc, 300, false);
    r = reconnect_adopt_epoch(&rc, 300, 9, false);
    CHECK(r.action == RECONNECT_ACTION_REPLACE);
    CHECK(r.session_replaced);
    CHECK(!r.link_up);
    CHECK(rc.state == RECONNECT_STATE_RECONNECTING);
    CHECK(rc.replacements == 2);
}

static void test_stale_session_cannot_coexist(void) {
    reconnect_t rc;
    reconnect_result_t r;

    reconnect_init(&rc);
    (void)reconnect_adopt_epoch(&rc, 0, 7, true);
    (void)reconnect_adopt_epoch(&rc, 10, 8, true);

    r = reconnect_accept_epoch(&rc, 20, 7);
    CHECK(r.stale_rejected);
    CHECK(rc.stale_rejected == 1);
    CHECK(r.action == RECONNECT_ACTION_LINK_DOWN);
    CHECK(r.state == RECONNECT_STATE_RECONNECTING);
    CHECK(!rc.session_active);
    CHECK(r.backoff_ms == RECONNECT_BACKOFF_MIN_MS);

    /* Without a live session no epoch is current; a newer one is not stale. */
    r = reconnect_accept_epoch(&rc, 20, 8);
    CHECK(!r.stale_rejected);
    r = reconnect_accept_epoch(&rc, 20, 9);
    CHECK(!r.stale_rejected);
    CHECK(rc.stale_rejected == 1);
}

static void test_epoch_is_monotonic(void) {
    reconnect_t rc;
    reconnect_result_t r;

    reconnect_init(&rc);
    (void)reconnect_adopt_epoch(&rc, 0, 5, true);

    r = reconnect_adopt_epoch(&rc, 10, 5, true);
    CHECK(r.stale_rejected);
    r = reconnect_adopt_epoch(&rc, 10, 4, true);
    CHECK(r.stale_rejected);
    CHECK(rc.stale_rejected == 2);
    CHECK(rc.epoch == 5);
    CHECK(rc.replacements == 0);

    /* Zero is the "no session" sentinel, never a generation. */
    r = reconnect_adopt_epoch(&rc, 10, 0, true);
    CHECK(r.action == RECONNECT_ACTION_NONE);
    CHECK(!r.stale_rejected);
    CHECK(rc.epoch == 5);
}

static void test_log_helpers_and_backoff_visibility(void) {
    reconnect_t rc;
    reconnect_result_t r;

    CHECK(strcmp(reconnect_state_name(RECONNECT_STATE_DOWN), "DOWN") == 0);
    CHECK(strcmp(reconnect_state_name(RECONNECT_STATE_CONNECTING),
                 "CONNECTING") == 0);
    CHECK(strcmp(reconnect_state_name(RECONNECT_STATE_RECONNECTING),
                 "RECONNECTING") == 0);
    CHECK(strcmp(reconnect_state_name(RECONNECT_STATE_UP), "UP") == 0);
    CHECK(strcmp(reconnect_action_name(RECONNECT_ACTION_NONE), "none") == 0);
    CHECK(strcmp(reconnect_action_name(RECONNECT_ACTION_START), "start") == 0);
    CHECK(strcmp(reconnect_action_name(RECONNECT_ACTION_RETRY), "retry") == 0);
    CHECK(strcmp(reconnect_action_name(RECONNECT_ACTION_REPLACE),
                 "replace") == 0);

    reconnect_init(&rc);
    (void)reconnect_adopt_epoch(&rc, 0, 1, true);
    (void)reconnect_tick(&rc, 0, false);
    r = reconnect_tick(&rc, 500, false);
    CHECK(r.backoff_ms >= RECONNECT_BACKOFF_MIN_MS);
    CHECK(r.backoff_ms <= RECONNECT_BACKOFF_MAX_MS);
    CHECK(strcmp(reconnect_action_name(r.action), "retry") == 0);
}

static void test_ten_power_cycles_recover(void) {
    reconnect_t rc;
    reconnect_result_t r;
    uint32_t cycle;

    for (cycle = 0; cycle < 10; cycle++) {
        uint32_t base = cycle * 100000u;
        uint32_t epoch = cycle + 1u;
        uint32_t recovery_ms;

        reconnect_init(&rc);
        CHECK(reconnect_tick(&rc, base, false).state ==
              RECONNECT_STATE_DOWN);
        CHECK(reconnect_tick(&rc, base + 1u, true).action ==
              RECONNECT_ACTION_START);
        CHECK(reconnect_tick(&rc, base + 501u, true).action ==
              RECONNECT_ACTION_START);
        (void)reconnect_adopt_epoch(&rc, base + 520u, epoch, true);
        r = reconnect_tick(&rc, base + 530u, true);
        CHECK(r.state == RECONNECT_STATE_UP);
        CHECK(r.action == RECONNECT_ACTION_NONE);
        CHECK(rc.epoch == epoch);
        recovery_ms = 530u;
        CHECK(recovery_ms < 10000u);
    }
}

static void test_retry_backoff_resets_on_success(void) {
    reconnect_t rc;
    reconnect_result_t r;

    reconnect_init(&rc);
    (void)reconnect_adopt_epoch(&rc, 0, 1, true);
    (void)reconnect_tick(&rc, 0, false);
    (void)reconnect_tick(&rc, 500, false);
    (void)reconnect_tick(&rc, 1500, false);
    CHECK(rc.backoff_ms == 2000);
    CHECK(rc.attempts == 2);

    r = reconnect_adopt_epoch(&rc, 1500, 2, true);
    CHECK(r.link_up);
    CHECK(rc.backoff_ms == 0);
    CHECK(rc.next_attempt_ms == 1500);
    CHECK(rc.attempts == 2);
}

static void test_time_wraparound(void) {
    reconnect_t rc;
    reconnect_result_t r;

    reconnect_init(&rc);
    (void)reconnect_adopt_epoch(&rc, UINT32_MAX - 1000u, 1, true);
    r = reconnect_tick(&rc, UINT32_MAX - 900u, false);
    CHECK(r.action == RECONNECT_ACTION_LINK_DOWN);

    r = reconnect_tick(&rc, UINT32_MAX - 500u, true);
    CHECK(r.action == RECONNECT_ACTION_NONE);
    r = reconnect_tick(&rc, 0, true);
    CHECK(r.action == RECONNECT_ACTION_RETRY);
}

int main(void) {
    test_initial_state();
    test_down_to_connecting();
    test_bounded_backoff();
    test_link_loss_enters_reconnecting();
    test_ap_restart_relinks_under_ten_seconds();
    test_sta_restart_replaces_session();
    test_stale_session_cannot_coexist();
    test_epoch_is_monotonic();
    test_log_helpers_and_backoff_visibility();
    test_ten_power_cycles_recover();
    test_retry_backoff_resets_on_success();
    test_time_wraparound();

    if (failures) {
        printf("reconnect: %d failure(s)\n", failures);
        return 1;
    }
    printf("reconnect: PASS\n");
    return 0;
}
