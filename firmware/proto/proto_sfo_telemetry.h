#ifndef PHANTOM_SFO_TELEMETRY_H
#define PHANTOM_SFO_TELEMETRY_H

/* Runtime telemetry for the application-layer SFO channel.
 * Telemetry never gates user message delivery. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SFO_DEGRADE_THRESHOLD 3u

typedef struct {
    uint32_t good;
    uint32_t total;
    uint32_t bits;
    uint32_t reorders;
    uint32_t consecutive_failures;
    bool degraded;
} sfo_telemetry_t;

/* Read-only copy for the UI/logging layer. */
typedef struct {
    uint32_t good;
    uint32_t total;
    uint32_t bits;
    uint32_t reorders;
    bool degraded;
} sfo_telemetry_snapshot_t;

void sfo_telemetry_init(sfo_telemetry_t *t);
void sfo_telemetry_record_success(sfo_telemetry_t *t, uint8_t nibble);
void sfo_telemetry_record_failure(sfo_telemetry_t *t);
void sfo_telemetry_record_reorder(sfo_telemetry_t *t);
void sfo_telemetry_snapshot(const sfo_telemetry_t *t,
                            sfo_telemetry_snapshot_t *out);
bool sfo_telemetry_is_degraded(const sfo_telemetry_t *t);
void sfo_telemetry_format(const sfo_telemetry_snapshot_t *s,
                          char *buf, size_t cap);

#endif
