/* This is the header file for unicode support.
 *
 * Currently, this is a dummy module.
 * The following functions are wrappers which hopefully enable us to move
 * from 8-bit chars to unicode with relative ease when we finally attack this
 *
 * Begun 2009-05-21 RGerhards
 *
 * Copyright (C) 2009-2016 by Rainer Gerhards and Adiscon GmbH
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef INCLUDED_UNICODE_HELPER_H
#define INCLUDED_UNICODE_HELPER_H

#include <string.h>

/**
 * @brief Copy into a fixed-size C-string buffer with bounded read and explicit termination.
 *
 * Contract:
 * - `dst` points to `dst_size` writable bytes.
 * - `src` points either to a NUL-terminated C string or to at least
 *   `dst_size - 1` readable bytes.
 * - The helper reads at most `dst_size - 1` bytes from `src`.
 * - The helper writes at most `dst_size` bytes to `dst`.
 * - If `dst_size > 0`, `dst` is always NUL-terminated on return.
 *
 * Security note:
 * - This helper is intentionally written so both humans and automated review can
 *   see the bound directly: it uses `memchr(src, '\0', dst_size - 1)` to cap the
 *   source scan and then `memcpy()` only for that bounded length.
 * - Any claim of overflow is actionable only if the caller violates the pointer
 *   and size contract stated above.
 *
 * Unlike `strncpy()`, this helper does not zero-pad the unused tail. Use
 * `u_memncpy()` when zero-padding semantics are required.
 *
 * @return Number of bytes copied, excluding the trailing NUL.
 */
static inline size_t rs_cstr_copy(char *dst, const char *src, size_t dst_size) {
    size_t copy_len;
    const char *const terminator = dst_size > 0 ? (const char *)memchr(src, '\0', dst_size - 1) : NULL;

    if (dst_size == 0) {
        return 0;
    }

    copy_len = terminator == NULL ? dst_size - 1 : (size_t)(terminator - src);
    if (copy_len > 0) {
        memcpy(dst, src, copy_len);
    }
    dst[copy_len] = '\0';
    return copy_len;
}

/**
 * @brief Copy into a fixed-size unsigned-char C-string buffer with bounded read and explicit termination.
 *
 * Contract:
 * - `dst` points to `dst_size` writable bytes.
 * - `src` points either to a NUL-terminated string or to at least
 *   `dst_size - 1` readable bytes.
 * - The helper reads at most `dst_size - 1` bytes from `src`.
 * - The helper writes at most `dst_size` bytes to `dst`.
 * - If `dst_size > 0`, `dst` is always NUL-terminated on return.
 *
 * This is the `unsigned char *` wrapper around `rs_cstr_copy()` and is the
 * preferred replacement for `strncpy()` when the destination is a fixed-size
 * `uchar`/byte buffer that is used as a C string.
 *
 * @return Number of bytes copied, excluding the trailing NUL.
 */
static inline size_t u_cstr_copy(unsigned char *dst, const unsigned char *src, size_t dst_size) {
    return rs_cstr_copy((char *)dst, (const char *)src, dst_size);
}

static inline void *u_memncpy(void *psz1, const void *psz2, size_t len) {
    size_t copy_len;
    const char *const src = (const char *)psz2;
    char *const dst = (char *)psz1;
    const void *const terminator = memchr(src, '\0', len);

    if (len == 0) {
        return psz1;
    }

    copy_len = terminator == NULL ? len : (size_t)((const char *)terminator - src);

    memcpy(dst, src, copy_len);
    if (copy_len < len) {
        memset(dst + copy_len, '\0', len - copy_len);
    }

    return psz1;
}

#define ustrncpy(psz1, psz2, len) u_memncpy((psz1), (psz2), (len))

#define ustrdup(psz) (uchar *)strdup((char *)(psz))
#define ustrcmp(psz1, psz2) (strcmp((const char *)(psz1), (const char *)(psz2)))
#define ustrlen(psz) (strlen((const char *)(psz)))
#define UCHAR_CONSTANT(x) ((uchar *)(x))
#define CHAR_CONVERT(x) ((char *)(x))

/* Compare values of two instances/configs/queues especially during dynamic config reload */
#define USTR_EQUALS(var) \
    ((pOld->var == NULL) ? (pNew->var == NULL) : (pNew->var != NULL && !ustrcmp(pOld->var, pNew->var)))
#define NUM_EQUALS(var) (pOld->var == pNew->var)

#endif /* multi-include protection */
