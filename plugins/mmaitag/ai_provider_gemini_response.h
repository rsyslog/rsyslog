/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file ai_provider_gemini_response.h
 * @brief Bounded response buffering for the mmaitag Gemini provider.
 */
#ifndef AI_PROVIDER_GEMINI_RESPONSE_H
#define AI_PROVIDER_GEMINI_RESPONSE_H

#include <stddef.h>

#define MMAITAG_GEMINI_MAX_RESPONSE_BYTES ((size_t)(1024U * 1024U))
#define MMAITAG_GEMINI_TOTAL_TIMEOUT_MS 60000L
#define MMAITAG_GEMINI_CONNECT_TIMEOUT_MS 10000L
#define MMAITAG_GEMINI_LOW_SPEED_LIMIT 1L
#define MMAITAG_GEMINI_LOW_SPEED_TIME 15L

struct mmaitag_gemini_response {
    char *buf;
    size_t len;
    size_t max_bytes;
    int limit_exceeded;
};

size_t mmaitag_gemini_response_write(char *ptr, size_t size, size_t nmemb, void *data);

#endif /* AI_PROVIDER_GEMINI_RESPONSE_H */
