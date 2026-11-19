#include <stdio.h>
#include <string.h>

#include "banner.h"

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

int main(void) {
    char buf[64];

    int n = phantom_banner(buf, sizeof buf, true);
    CHECK(strcmp(buf, "phantom-link v1 testhash AP") == 0);
    CHECK(n == (int)strlen("phantom-link v1 testhash AP"));

    n = phantom_banner(buf, sizeof buf, false);
    CHECK(strcmp(buf, "phantom-link v1 testhash STA") == 0);
    CHECK(n == (int)strlen("phantom-link v1 testhash STA"));

    /* A short buffer must truncate and still NUL-terminate. */
    memset(buf, 'x', sizeof buf);
    n = phantom_banner(buf, 8, true);
    CHECK(n == (int)strlen("phantom-link v1 testhash AP"));
    CHECK(strcmp(buf, "phantom") == 0);

    /* A one-byte buffer writes only the terminator. */
    memset(buf, 'x', sizeof buf);
    n = phantom_banner(buf, 1, true);
    CHECK(n == (int)strlen("phantom-link v1 testhash AP"));
    CHECK(buf[0] == '\0');

    if (failures) {
        printf("banner: %d failure(s)\n", failures);
        return 1;
    }
    printf("banner: PASS\n");
    return 0;
}
