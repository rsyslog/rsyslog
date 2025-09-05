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
#ifndef HAVE_ASPRINTF

    #include <stdlib.h>
    #include <stdarg.h>
    #include <stdio.h>
int asprintf(char **strp, const char *fmt, ...) {
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    *strp = malloc(len + 1);
    if (!*strp) {
        return -1;
    }

    va_start(ap, fmt);
    vsnprintf(*strp, len + 1, fmt, ap);
    va_end(ap);

    (*strp)[len] = 0;
    return len;
}
#else
    /* XLC needs at least one method in source file even static to compile */
    #ifdef __xlc__
static void dummy(void) {}
    #endif
#endif /* #ifndef HAVE_ASPRINTF */
