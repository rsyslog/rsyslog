/* compatibility file for systems without asprintf.
 *
 * Copyright 2019 P Duveau
 *
 * This file is part of rsyslog.
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
#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#if !defined(HAVE_VASPRINTF) || !defined(HAVE_ASPRINTF)
static int compat_vasprintf_impl(char **strp, const char *fmt, va_list ap)
    #if defined(__GNUC__)
    __attribute__((format(printf, 2, 0)))
    #endif
    ;

static int compat_vasprintf_impl(char **strp, const char *fmt, va_list ap) {
    va_list ap_copy;
    int len;
    char *buf;

    if (strp == NULL || fmt == NULL) {
        return -1;
    }

    *strp = NULL;

    va_copy(ap_copy, ap);
    len = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);
    if (len < 0) {
        return -1;
    }

    buf = malloc((size_t)len + 1);
    if (buf == NULL) {
        return -1;
    }

    va_copy(ap_copy, ap);
    if (vsnprintf(buf, (size_t)len + 1, fmt, ap_copy) != len) {
        va_end(ap_copy);
        free(buf);
        return -1;
    }
    va_end(ap_copy);

    *strp = buf;
    return len;
}
#endif

#ifndef HAVE_VASPRINTF
int vasprintf(char **strp, const char *fmt, va_list ap) {
    return compat_vasprintf_impl(strp, fmt, ap);
}
#endif

#ifndef HAVE_ASPRINTF
int asprintf(char **strp, const char *fmt, ...) {
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = compat_vasprintf_impl(strp, fmt, ap);
    va_end(ap);

    return ret;
}
#endif
