/* SPDX-License-Identifier: Apache-2.0 */

/* gss-token-util.h
 *
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef GSS_TOKEN_UTIL_H_INCLUDED
#define GSS_TOKEN_UTIL_H_INCLUDED 1

/**
 * @file gss-token-util.h
 * @brief Shared helpers for bounded GSS token framing in rsyslog.
 *
 * This header centralizes small, header-only helpers that validate GSS wire
 * framing and derive conservative token-size limits. It exists so the common
 * GSS runtime helper and both GSS modules use the same length-decoding and
 * size-bound logic instead of open-coding slightly different checks.
 *
 * Current users include:
 * - `runtime/gss-misc.c` for framed token receive time/size enforcement
 * - `plugins/imgssapi/imgssapi.c` for server-side handshake/message limits
 * - `plugins/omgssapi/omgssapi.c` for client-side handshake limits
 */

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "rsyslog.h"

#define GSS_TOKEN_MAX_HANDSHAKE_BYTES (1024U * 1024U)
#define GSS_TOKEN_MAX_WRAP_OVERHEAD_BYTES (64U * 1024U)
#define GSS_TOKEN_IO_TIMEOUT_SECS 10U

/**
 * @brief Decode a 4-byte GSS wire length field and validate it against a limit.
 *
 * GSS token framing in rsyslog uses a fixed 4-byte network-order length prefix.
 * Callers use this helper before allocating buffers or attempting to read token
 * bodies from the peer.
 *
 * @param[in]  lenbuf     Four bytes containing the network-order token length.
 * @param[in]  max_len    Maximum token size the caller is willing to accept.
 * @param[out] token_len  Decoded token length on success.
 *
 * @retval RS_RET_OK              Length decoded and within @p max_len.
 * @retval RS_RET_INVALID_PARAMS  @p token_len is NULL.
 * @retval RS_RET_INVALID_VALUE   Decoded length exceeds @p max_len.
 */
static inline rsRetVal gssTokenDecodeLength(const unsigned char lenbuf[4], size_t max_len, size_t *const token_len) {
    const uint32_t wire_len =
        ((uint32_t)lenbuf[0] << 24) | ((uint32_t)lenbuf[1] << 16) | ((uint32_t)lenbuf[2] << 8) | (uint32_t)lenbuf[3];

    if (token_len == NULL) {
        return RS_RET_INVALID_PARAMS;
    }

    *token_len = (size_t)wire_len;
    if ((size_t)wire_len > max_len) {
        return RS_RET_INVALID_VALUE;
    }

    return RS_RET_OK;
}

/**
 * @brief Derive a safe wrapped-token size limit from a plaintext message limit.
 *
 * Wrapped GSS messages carry protocol overhead beyond the unwrapped syslog
 * payload. Receivers use this helper to cap the framed token size while still
 * accepting valid wrapped messages for a given plaintext maximum.
 *
 * @param[in] max_plaintext_len  Maximum accepted plaintext message length.
 *
 * @return The wrapped-token limit derived from @p max_plaintext_len. If adding
 *         the configured overhead would overflow `size_t`, `SIZE_MAX` is
 *         returned instead.
 */
static inline size_t gssTokenGetMessageLimit(size_t max_plaintext_len) {
    if (max_plaintext_len > SIZE_MAX - GSS_TOKEN_MAX_WRAP_OVERHEAD_BYTES) {
        return SIZE_MAX;
    }

    return max_plaintext_len + GSS_TOKEN_MAX_WRAP_OVERHEAD_BYTES;
}

#endif
