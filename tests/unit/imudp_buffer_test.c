/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
 */

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
    size_t slot_size = 0;
    size_t buffer_size = 0;

    CHECK(imudp_compute_buffer_layout(8192, 32, &slot_size, &buffer_size));
    CHECK(slot_size == 8193);
    CHECK(buffer_size == 262176);
    return 0;
}

static int test_large_layout_does_not_wrap_to_int(void) {
    size_t slot_size = 0;
    size_t buffer_size = 0;

#if SIZE_MAX > UINT32_MAX
    CHECK(imudp_compute_buffer_layout(134217728, 32, &slot_size, &buffer_size));
    CHECK(slot_size == 134217729);
    CHECK(buffer_size == UINT64_C(4294967328));
#else
    CHECK(!imudp_compute_buffer_layout(134217728, 32, &slot_size, &buffer_size));
#endif
    return 0;
}

static int test_int_max_single_message_layout(void) {
    size_t slot_size = 0;
    size_t buffer_size = 0;

    CHECK(imudp_compute_buffer_layout((size_t)INT_MAX, 1, &slot_size, &buffer_size));
    CHECK(slot_size == (size_t)INT_MAX + 1);
    CHECK(buffer_size == slot_size);
    return 0;
}

static int test_rejects_unrepresentable_layouts(void) {
    size_t slot_size = 0;
    size_t buffer_size = 0;

    CHECK(!imudp_compute_buffer_layout(SIZE_MAX, 1, &slot_size, &buffer_size));
    CHECK(!imudp_compute_buffer_layout(SIZE_MAX - 1, 2, &slot_size, &buffer_size));
    CHECK(!imudp_compute_buffer_layout(8192, 0, &slot_size, &buffer_size));
    CHECK(!imudp_compute_buffer_layout(8192, 32, NULL, &buffer_size));
    CHECK(!imudp_compute_buffer_layout(8192, 32, &slot_size, NULL));
    return 0;
}

static int test_checked_multiply_boundaries(void) {
    size_t result = 1;

    CHECK(imudp_checked_size_mul(0, SIZE_MAX, &result));
    CHECK(result == 0);
    CHECK(imudp_checked_size_mul(SIZE_MAX, 1, &result));
    CHECK(result == SIZE_MAX);
    CHECK(!imudp_checked_size_mul(SIZE_MAX, 2, &result));
    CHECK(!imudp_checked_size_mul(1, 1, NULL));
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
