#ifndef PHANTOM_BANNER_H
#define PHANTOM_BANNER_H

/*
 * The one-line boot banner. Kept in a header so the host tests can assert the
 * exact format without pulling in the Pico SDK.
 *
 * The build system defines PHANTOM_BUILD_HASH as the first eight hex digits of
 * main.c's SHA256. A local build without that definition still compiles and
 * says "dev", which makes a stale host binary obvious.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "proto/proto_spec.h"

#ifndef PHANTOM_BUILD_HASH
#define PHANTOM_BUILD_HASH "dev"
#endif

static inline int phantom_banner(char *buf, size_t cap, bool is_ap) {
    return snprintf(buf, cap, "phantom-link v%u %s %s",
                    (unsigned)PROTO_VERSION, PHANTOM_BUILD_HASH,
                    is_ap ? "AP" : "STA");
}

#endif
