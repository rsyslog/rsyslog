/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file ai_provider_gemini_response.c
 * @brief Bounded response buffering for the mmaitag Gemini provider.
 */
#include "config.h"
#include "ai_provider_gemini_response.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

size_t mmaitag_gemini_response_write(char *ptr, size_t size, size_t nmemb, void *data) {
    struct mmaitag_gemini_response *const response = (struct mmaitag_gemini_response *)data;
    size_t chunk_bytes;
    char *new_buf;

    if (response == NULL) return 0;
    if (size != 0 && nmemb > SIZE_MAX / size) {
        response->limit_exceeded = 1;
        return 0;
    }

    chunk_bytes = size * nmemb;
    if (chunk_bytes == 0) return 0;
    if (response->len > response->max_bytes || chunk_bytes > response->max_bytes - response->len ||
        chunk_bytes == SIZE_MAX || response->len > SIZE_MAX - chunk_bytes - 1) {
        response->limit_exceeded = 1;
        return 0;
    }

    new_buf = realloc(response->buf, response->len + chunk_bytes + 1);
    if (new_buf == NULL) return 0;

    response->buf = new_buf;
    memcpy(response->buf + response->len, ptr, chunk_bytes);
    response->len += chunk_bytes;
    response->buf[response->len] = '\0';
    return chunk_bytes;
}
