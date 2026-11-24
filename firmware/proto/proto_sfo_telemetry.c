#include "proto_sfo_telemetry.h"

#include <stdio.h>
#include <string.h>

void sfo_telemetry_init(sfo_telemetry_t *t) {
    if (!t) return;
    memset(t, 0, sizeof *t);
}

void sfo_telemetry_record_success(sfo_telemetry_t *t, uint8_t nibble) {
    if (!t) return;
    (void)nibble;
    t->good++;
    t->total++;
    t->bits += 4u;
    t->consecutive_failures = 0u;
    t->degraded = false;
}

void sfo_telemetry_record_failure(sfo_telemetry_t *t) {
    if (!t) return;
    t->total++;
    t->consecutive_failures++;
    if (t->consecutive_failures >= SFO_DEGRADE_THRESHOLD) t->degraded = true;
}

void sfo_telemetry_record_reorder(sfo_telemetry_t *t) {
    if (!t) return;
    t->reorders++;
}

void sfo_telemetry_snapshot(const sfo_telemetry_t *t,
                            sfo_telemetry_snapshot_t *out) {
    if (!t || !out) return;
    out->good = t->good;
    out->total = t->total;
    out->bits = t->bits;
    out->reorders = t->reorders;
    out->degraded = t->degraded;
}

bool sfo_telemetry_is_degraded(const sfo_telemetry_t *t) {
    return t && t->degraded;
}

void sfo_telemetry_format(const sfo_telemetry_snapshot_t *s,
                          char *buf, size_t cap) {
    if (!s || !buf || cap == 0u) return;
    snprintf(buf, cap,
             "SFO good=%u/%u bps=%u reorder=%u degraded=%u",
             (unsigned)s->good, (unsigned)s->total, (unsigned)s->bits,
             (unsigned)s->reorders, s->degraded ? 1u : 0u);
}
