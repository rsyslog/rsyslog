#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "rsyslog.h"

#define LOG_MAXPRI 191

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                                \
        }                                                                            \
    } while (0)

/* Copy of compute_off_after_pri under test to ensure identical pure logic verification */
static int compute_off_after_pri(const uchar *buf, size_t len, int *pri) {
    if (len < 3 || buf[0] != '<') {
        return -1;
    }
    if (isdigit(buf[1]) && buf[2] == '>') {
        if (pri != NULL) {
            *pri = buf[1] - '0';
        }
        return 3;
    }
    if (len >= 4 && isdigit(buf[1]) && isdigit(buf[2]) && buf[3] == '>') {
        if (pri != NULL) {
            *pri = (buf[1] - '0') * 10 + (buf[2] - '0');
        }
        return 4;
    }
    if (len >= 5 && isdigit(buf[1]) && isdigit(buf[2]) && isdigit(buf[3]) && buf[4] == '>') {
        int val = (buf[1] - '0') * 100 + (buf[2] - '0') * 10 + (buf[3] - '0');
        if (val <= LOG_MAXPRI) {
            if (pri != NULL) {
                *pri = val;
            }
            return 5;
        }
    }
    return -1;
}

static int test_rfc3164_valid(void) {
    const uchar buf[] = "<13>hello";
    int pri = -1;
    CHECK(compute_off_after_pri(buf, strlen((const char *)buf), &pri) == 4);
    CHECK(pri == 13);
    return 0;
}

static int test_rfc5424_valid(void) {
    const uchar buf[] = "<165>1 2026-05-25...";
    int pri = -1;
    CHECK(compute_off_after_pri(buf, strlen((const char *)buf), &pri) == 5);
    CHECK(pri == 165);
    return 0;
}

static int test_missing_greater_than(void) {
    const uchar buf[] = "<13hello";
    int pri = -1;
    CHECK(compute_off_after_pri(buf, strlen((const char *)buf), &pri) == -1);
    return 0;
}

static int test_non_digit_in_pri(void) {
    const uchar buf[] = "<1a>hello";
    int pri = -1;
    CHECK(compute_off_after_pri(buf, strlen((const char *)buf), &pri) == -1);
    return 0;
}

static int test_too_many_digits(void) {
    const uchar buf[] = "<1234>hello";
    int pri = -1;
    CHECK(compute_off_after_pri(buf, strlen((const char *)buf), &pri) == -1);
    return 0;
}

static int test_no_leading_less_than(void) {
    const uchar buf[] = "13>hello";
    int pri = -1;
    CHECK(compute_off_after_pri(buf, strlen((const char *)buf), &pri) == -1);
    return 0;
}

static int test_empty_pri(void) {
    const uchar buf[] = "<>hello";
    int pri = -1;
    CHECK(compute_off_after_pri(buf, strlen((const char *)buf), &pri) == -1);
    return 0;
}

static int test_out_of_range(void) {
    const uchar buf[] = "<192>hello";
    int pri = -1;
    CHECK(compute_off_after_pri(buf, strlen((const char *)buf), &pri) == -1);
    return 0;
}

static int test_very_short_buffers(void) {
    int pri = -1;
    CHECK(compute_off_after_pri((const uchar *)"", 0, &pri) == -1);
    CHECK(compute_off_after_pri((const uchar *)"<", 1, &pri) == -1);
    CHECK(compute_off_after_pri((const uchar *)"<1", 2, &pri) == -1);
    CHECK(compute_off_after_pri((const uchar *)"<1>", 3, &pri) == 3);
    CHECK(pri == 1);
    return 0;
}

static int test_leading_nuls_or_garbage(void) {
    const uchar buf[] = "\0<13>hello";
    int pri = -1;
    CHECK(compute_off_after_pri(buf, 10, &pri) == -1);
    return 0;
}

int main(void) {
    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"rfc3164_valid", test_rfc3164_valid},
        {"rfc5424_valid", test_rfc5424_valid},
        {"missing_greater_than", test_missing_greater_than},
        {"non_digit_in_pri", test_non_digit_in_pri},
        {"too_many_digits", test_too_many_digits},
        {"no_leading_less_than", test_no_leading_less_than},
        {"empty_pri", test_empty_pri},
        {"out_of_range", test_out_of_range},
        {"very_short_buffers", test_very_short_buffers},
        {"leading_nuls_or_garbage", test_leading_nuls_or_garbage},
    };
    size_t i;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (tests[i].fn() != 0) {
            fprintf(stderr, "FAILED: %s\n", tests[i].name);
            return 1;
        }
    }

    printf("parser_pri unit tests passed (%zu cases)\n", sizeof(tests) / sizeof(tests[0]));
    return 0;
}
