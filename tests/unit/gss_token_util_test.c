#include "config.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include "rsyslog.h"
#include "gss-token-util.h"

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                                \
        }                                                                            \
    } while (0)

static int test_decode_length_accepts_small_frame(void) {
    static const unsigned char lenbuf[4] = {0x00, 0x00, 0x01, 0x00};
    size_t token_len = 0;

    CHECK(gssTokenDecodeLength(lenbuf, 512, &token_len) == RS_RET_OK);
    CHECK(token_len == 256);

    return 0;
}

static int test_decode_length_rejects_oversize_frame(void) {
    static const unsigned char lenbuf[4] = {0x00, 0x10, 0x00, 0x00};
    size_t token_len = 0;

    CHECK(gssTokenDecodeLength(lenbuf, 4096, &token_len) == RS_RET_INVALID_VALUE);
    CHECK(token_len == 0x00100000U);

    return 0;
}

static int test_decode_length_rejects_null_output_pointer(void) {
    static const unsigned char lenbuf[4] = {0x00, 0x00, 0x00, 0x01};

    CHECK(gssTokenDecodeLength(lenbuf, 1, NULL) == RS_RET_INVALID_PARAMS);
    return 0;
}

static int test_message_limit_adds_wrap_overhead(void) {
    CHECK(gssTokenGetMessageLimit(4096) == 4096 + GSS_TOKEN_MAX_WRAP_OVERHEAD_BYTES);
    return 0;
}

static int test_message_limit_saturates_on_overflow(void) {
    CHECK(gssTokenGetMessageLimit(SIZE_MAX) == SIZE_MAX);
    CHECK(gssTokenGetMessageLimit(SIZE_MAX - 1) == SIZE_MAX);
    return 0;
}

int main(void) {
    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"decode_length_accepts_small_frame", test_decode_length_accepts_small_frame},
        {"decode_length_rejects_oversize_frame", test_decode_length_rejects_oversize_frame},
        {"decode_length_rejects_null_output_pointer", test_decode_length_rejects_null_output_pointer},
        {"message_limit_adds_wrap_overhead", test_message_limit_adds_wrap_overhead},
        {"message_limit_saturates_on_overflow", test_message_limit_saturates_on_overflow},
    };
    size_t i;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (tests[i].fn() != 0) {
            fprintf(stderr, "FAILED: %s\n", tests[i].name);
            return 1;
        }
    }

    printf("gss token util tests passed (%zu cases)\n", sizeof(tests) / sizeof(tests[0]));
    return 0;
}
