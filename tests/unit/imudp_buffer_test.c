#include "config.h"

/* Verify imudp receive-layout arithmetic without allocating the large boundary cases. */

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include "imudp_buffer.h"

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                                \
        }                                                                            \
    } while (0)

static int test_default_layout(void) {
    size_t slotSize = 0;
    size_t bufferSize = 0;

    CHECK(imudpComputeBufferLayout(8192, 32, &slotSize, &bufferSize));
    CHECK(slotSize == 8193);
    CHECK(bufferSize == 262176);
    return 0;
}

static int test_large_layout_does_not_wrap_to_int(void) {
    size_t slotSize = 0;
    size_t bufferSize = 0;

#if SIZE_MAX > UINT32_MAX
    CHECK(imudpComputeBufferLayout(134217728, 32, &slotSize, &bufferSize));
    CHECK(slotSize == 134217729);
    CHECK(bufferSize == UINT64_C(4294967328));
#else
    CHECK(!imudpComputeBufferLayout(134217728, 32, &slotSize, &bufferSize));
#endif
    return 0;
}

static int test_int_max_single_message_layout(void) {
    size_t slotSize = 0;
    size_t bufferSize = 0;

    CHECK(imudpComputeBufferLayout((size_t)INT_MAX, 1, &slotSize, &bufferSize));
    CHECK(slotSize == (size_t)INT_MAX + 1);
    CHECK(bufferSize == slotSize);
    return 0;
}

static int test_rejects_unrepresentable_layouts(void) {
    size_t slotSize = 0;
    size_t bufferSize = 0;

    CHECK(!imudpComputeBufferLayout(SIZE_MAX, 1, &slotSize, &bufferSize));
    CHECK(!imudpComputeBufferLayout(SIZE_MAX - 1, 2, &slotSize, &bufferSize));
    CHECK(!imudpComputeBufferLayout(8192, 0, &slotSize, &bufferSize));
    CHECK(!imudpComputeBufferLayout(8192, 32, NULL, &bufferSize));
    CHECK(!imudpComputeBufferLayout(8192, 32, &slotSize, NULL));
    return 0;
}

static int test_checked_multiply_boundaries(void) {
    size_t result = 1;

    CHECK(imudpCheckedSizeMul(0, SIZE_MAX, &result));
    CHECK(result == 0);
    CHECK(imudpCheckedSizeMul(SIZE_MAX, 1, &result));
    CHECK(result == SIZE_MAX);
    CHECK(!imudpCheckedSizeMul(SIZE_MAX, 2, &result));
    CHECK(!imudpCheckedSizeMul(1, 1, NULL));
    return 0;
}

int main(void) {
    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"default_layout", test_default_layout},
        {"large_layout_does_not_wrap_to_int", test_large_layout_does_not_wrap_to_int},
        {"int_max_single_message_layout", test_int_max_single_message_layout},
        {"rejects_unrepresentable_layouts", test_rejects_unrepresentable_layouts},
        {"checked_multiply_boundaries", test_checked_multiply_boundaries},
    };
    size_t i;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (tests[i].fn() != 0) {
            fprintf(stderr, "FAILED: %s\n", tests[i].name);
            return 1;
        }
    }

    printf("imudp buffer tests passed (%zu cases)\n", sizeof(tests) / sizeof(tests[0]));
    return 0;
}
