#include "config.h"

/* Verify ommongodb's ISODate parser preserves BSON millisecond precision.
 * The oracle is direct epoch-millisecond comparison for UTC, numeric-offset,
 * fractional-second, legacy :UTC, and invalid-input cases.
 */
#include <stdint.h>
#include <stdio.h>

#include "ommongodb_date.h"

/*
 * Pull the implementation into this test translation unit so Automake does not
 * need to manage dependency files for sources outside tests/ during distcheck.
 */
#include "../../plugins/ommongodb/ommongodb_date.c"

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                                \
        }                                                                            \
    } while (0)

static int expect_date(const char *date, int64_t expected) {
    int64_t actual;

    actual = -1;
    CHECK(ommongodbParseIsoDateMs(date, &actual) == 1);
    CHECK(actual == expected);
    return 0;
}

static int expect_invalid(const char *date) {
    int64_t actual;

    actual = -1;
    CHECK(ommongodbParseIsoDateMs(date, &actual) == 0);
    CHECK(actual == -1);
    return 0;
}

int main(void) {
    CHECK(expect_date("2019-10-20T12:00:19Z", 1571572819000LL) == 0);
    CHECK(expect_date("2019-10-20T12:00:19.1Z", 1571572819100LL) == 0);
    CHECK(expect_date("2019-10-20T12:00:19.123Z", 1571572819123LL) == 0);
    CHECK(expect_date("2019-10-20T12:00:19.123456Z", 1571572819123LL) == 0);
    CHECK(expect_date("2019-10-20T12:00:19.123+02:00", 1571565619123LL) == 0);
    CHECK(expect_date("2019-10-20T12:00:19.123+0200", 1571565619123LL) == 0);
    CHECK(expect_date("2019-10-20T12:00:19:UTC", 1571572819000LL) == 0);

    CHECK(expect_invalid("2019-10-20T12:00:19.Z") == 0);
    CHECK(expect_invalid("2019-02-29T12:00:19Z") == 0);
    CHECK(expect_invalid("2019-10-20T12:00:19") == 0);
    CHECK(expect_invalid("2019") == 0);
    CHECK(expect_invalid("2019-10-20T12:00:19U") == 0);
    return 0;
}
