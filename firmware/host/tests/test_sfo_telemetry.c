#include <stdio.h>
#include <string.h>

#include "proto_sfo_telemetry.h"

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static void test_init_zeroes_everything(void) {
    sfo_telemetry_t t;
    sfo_telemetry_snapshot_t s;

    sfo_telemetry_init(&t);
    sfo_telemetry_snapshot(&t, &s);
    CHECK(s.good == 0u);
    CHECK(s.total == 0u);
    CHECK(s.bits == 0u);
    CHECK(s.reorders == 0u);
    CHECK(s.degraded == false);
}

static void test_success_counts(void) {
    sfo_telemetry_t t;
    sfo_telemetry_snapshot_t s;

    sfo_telemetry_init(&t);
    sfo_telemetry_record_success(&t, 0x0Au);
    sfo_telemetry_record_success(&t, 0x0Bu);
    sfo_telemetry_snapshot(&t, &s);
    CHECK(s.good == 2u);
    CHECK(s.total == 2u);
    CHECK(s.bits == 8u);
    CHECK(s.degraded == false);
}

static void test_degraded_mode(void) {
    sfo_telemetry_t t;
    sfo_telemetry_snapshot_t s;

    sfo_telemetry_init(&t);
    sfo_telemetry_record_failure(&t);
    sfo_telemetry_record_failure(&t);
    CHECK(sfo_telemetry_is_degraded(&t) == false);
    sfo_telemetry_record_failure(&t);
    CHECK(sfo_telemetry_is_degraded(&t) == true);
    sfo_telemetry_snapshot(&t, &s);
    CHECK(s.degraded == true);
    CHECK(s.total == 3u);

    sfo_telemetry_record_success(&t, 0x01u);
    CHECK(sfo_telemetry_is_degraded(&t) == false);
    sfo_telemetry_snapshot(&t, &s);
    CHECK(s.degraded == false);
}

static void test_failure_and_reorder_counts(void) {
    sfo_telemetry_t t;
    sfo_telemetry_snapshot_t s;

    sfo_telemetry_init(&t);
    sfo_telemetry_record_failure(&t);
    sfo_telemetry_record_reorder(&t);
    sfo_telemetry_snapshot(&t, &s);
    CHECK(s.good == 0u);
    CHECK(s.total == 1u);
    CHECK(s.reorders == 1u);
}

static void test_delivery_independence(void) {
    sfo_telemetry_t t;
    char baseline[4] = { 'A', 'B', 'C', 'D' };
    char observed[4];
    size_t i;

    sfo_telemetry_init(&t);
    for (i = 0; i < 4; i++) {
        sfo_telemetry_record_failure(&t);
        observed[i] = baseline[i];
    }
    CHECK(sfo_telemetry_is_degraded(&t) == true);
    CHECK(memcmp(observed, baseline, sizeof baseline) == 0);
}

static void test_report_format(void) {
    sfo_telemetry_t t;
    sfo_telemetry_snapshot_t s;
    char buf[96];

    sfo_telemetry_init(&t);
    sfo_telemetry_record_success(&t, 0x0Au);
    sfo_telemetry_record_success(&t, 0x0Bu);
    sfo_telemetry_record_failure(&t);
    sfo_telemetry_record_reorder(&t);
    sfo_telemetry_snapshot(&t, &s);
    sfo_telemetry_format(&s, buf, sizeof buf);
    CHECK(strcmp(buf, "SFO good=2/3 bps=8 reorder=1 degraded=0") == 0);
}

int main(void) {
    test_init_zeroes_everything();
    test_success_counts();
    test_failure_and_reorder_counts();
    test_degraded_mode();
    test_delivery_independence();
    test_report_format();

    if (failures) {
        printf("sfo_telemetry: %d failure(s)\n", failures);
        return 1;
    }
    printf("sfo_telemetry: PASS\n");
    return 0;
}
