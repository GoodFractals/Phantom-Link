#include "proto_zeroize.h"

void zeroize(void *buf, size_t len) {
    volatile unsigned char *p = (volatile unsigned char *)buf;
    size_t i;

    if (p == NULL) {
        return;
    }
    for (i = 0; i < len; i++) {
        p[i] = (unsigned char)0;
    }
}

void zeroize_tracker_init(zeroize_tracker_t *tracker) {
    if (tracker == NULL) {
        return;
    }
    /* Scrub stale pointers from a previous lifetime without touching the
     * buffers they used to reference. */
    zeroize(tracker, sizeof(*tracker));
    tracker->count = 0u;
}

bool zeroize_tracker_register(zeroize_tracker_t *tracker, void *buf,
                              size_t len) {
    if (tracker == NULL || buf == NULL || len == 0u ||
        tracker->count >= (size_t)ZEROIZE_TRACKER_MAX) {
        return false;
    }

    tracker->entries[tracker->count].buf = buf;
    tracker->entries[tracker->count].len = len;
    tracker->count++;
    return true;
}

void zeroize_tracker_clear_all(zeroize_tracker_t *tracker) {
    size_t n;
    size_t i;

    if (tracker == NULL) {
        return;
    }

    /* Clamp defensively so a corrupted count can never walk off the array. */
    n = tracker->count;
    if (n > (size_t)ZEROIZE_TRACKER_MAX) {
        n = (size_t)ZEROIZE_TRACKER_MAX;
    }
    for (i = 0; i < n; i++) {
        zeroize(tracker->entries[i].buf, tracker->entries[i].len);
    }

    /* Drop the recorded pointers as well, then mark the tracker empty. */
    zeroize(tracker->entries, sizeof(tracker->entries));
    tracker->count = 0u;
}

size_t zeroize_tracker_count(const zeroize_tracker_t *tracker) {
    return (tracker == NULL) ? 0u : tracker->count;
}
