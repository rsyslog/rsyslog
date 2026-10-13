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

#define MMAITAG_GEMINI_MAX_CONFIG_RESPONSE_BYTES ((size_t)(64U * 1024U * 1024U))
#define MMAITAG_GEMINI_MAX_CONFIG_TIMEOUT_MS 3600000L
#define MMAITAG_GEMINI_MAX_CONFIG_LOW_SPEED_LIMIT (1024L * 1024L)
#define MMAITAG_GEMINI_MAX_CONFIG_LOW_SPEED_TIME 3600L

struct mmaitag_gemini_response {
    char *buf;
    size_t len;
    size_t max_bytes;
    int limit_exceeded;
};

size_t mmaitag_gemini_response_write(char *ptr, size_t size, size_t nmemb, void *data);

#endif /* AI_PROVIDER_GEMINI_RESPONSE_H */
