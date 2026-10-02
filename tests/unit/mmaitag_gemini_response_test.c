/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright 2026 Adiscon GmbH.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 * -or-
 * see COPYING.ASL20 in the source distribution
 */
#include "config.h"

/* Verify that Gemini response accumulation accepts data through the exact
 * byte boundary, rejects the next byte without modifying retained data, and
 * detects size_t multiplication overflow before reading the supplied buffer.
 * Direct callback return values and buffer state are the deterministic oracle;
 * no external provider, socket, or timing threshold is involved.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai_provider_gemini_response.h"

/* Compile the production response accumulator directly so distcheck does not
 * need an Automake dependency path outside tests/ for this unit binary.
 */
#include "../../plugins/mmaitag/ai_provider_gemini_response.c"

#define CHECK(condition)                                                                  \
    do {                                                                                  \
        if (!(condition)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                                     \
        }                                                                                 \
    } while (0)

int main(void) {
    struct mmaitag_gemini_response response = {.max_bytes = 8};
    struct mmaitag_gemini_response multiply_overflow = {.max_bytes = SIZE_MAX};
    struct mmaitag_gemini_response add_overflow = {.max_bytes = SIZE_MAX};

    CHECK(MMAITAG_GEMINI_MAX_RESPONSE_BYTES > 0);
    CHECK(MMAITAG_GEMINI_TOTAL_TIMEOUT_MS > 0);
    CHECK(MMAITAG_GEMINI_CONNECT_TIMEOUT_MS > 0);
    CHECK(MMAITAG_GEMINI_CONNECT_TIMEOUT_MS <= MMAITAG_GEMINI_TOTAL_TIMEOUT_MS);
    CHECK(MMAITAG_GEMINI_LOW_SPEED_LIMIT > 0);
    CHECK(MMAITAG_GEMINI_LOW_SPEED_TIME > 0);
    CHECK(MMAITAG_GEMINI_MAX_RESPONSE_BYTES <= MMAITAG_GEMINI_MAX_CONFIG_RESPONSE_BYTES);
    CHECK(MMAITAG_GEMINI_TOTAL_TIMEOUT_MS <= MMAITAG_GEMINI_MAX_CONFIG_TIMEOUT_MS);
    CHECK(MMAITAG_GEMINI_CONNECT_TIMEOUT_MS <= MMAITAG_GEMINI_MAX_CONFIG_TIMEOUT_MS);
    CHECK(MMAITAG_GEMINI_LOW_SPEED_LIMIT <= MMAITAG_GEMINI_MAX_CONFIG_LOW_SPEED_LIMIT);
    CHECK(MMAITAG_GEMINI_LOW_SPEED_TIME <= MMAITAG_GEMINI_MAX_CONFIG_LOW_SPEED_TIME);

    CHECK(mmaitag_gemini_response_write("abcd", 1, 4, &response) == 4);
    CHECK(mmaitag_gemini_response_write("efgh", 2, 2, &response) == 4);
    CHECK(response.len == response.max_bytes);
    CHECK(strcmp(response.buf, "abcdefgh") == 0);
    CHECK(response.limit_exceeded == 0);

    CHECK(mmaitag_gemini_response_write("i", 1, 1, &response) == 0);
    CHECK(response.limit_exceeded == 1);
    CHECK(response.len == response.max_bytes);
    CHECK(strcmp(response.buf, "abcdefgh") == 0);

    CHECK(mmaitag_gemini_response_write("x", SIZE_MAX, 2, &multiply_overflow) == 0);
    CHECK(multiply_overflow.limit_exceeded == 1);
    CHECK(multiply_overflow.buf == NULL);
    CHECK(multiply_overflow.len == 0);

    CHECK(mmaitag_gemini_response_write("x", 1, SIZE_MAX, &add_overflow) == 0);
    CHECK(add_overflow.limit_exceeded == 1);
    CHECK(add_overflow.buf == NULL);
    CHECK(add_overflow.len == 0);

    free(response.buf);
    return 0;
}
