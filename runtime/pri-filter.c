/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * PRI selector parsing descends from Eric Allman's Berkeley syslogd
 * (cfline(), with extensive changes by Ralph Campbell).
 * This port uses the selector portion of sysklogd's BSD-3-Clause cfline():
 * https://github.com/troglobit/sysklogd/blob/766f61d5b9ccc76efe606747990ee657add8d0f4/src/syslogd.c#L3748-L3880
 *
 * Adapted to rsyslog's name tables, diagnostics and priority masks.
 * rsyslog adaptations are also distributed under BSD-3-Clause.
 */
#include "config.h"
#include <assert.h>
#include <string.h>

#include "rsyslog.h"
#include "conf.h"
#include "errmsg.h"
#include "srUtils.h"

static uchar applyPrioritySelector(uchar mask, int pri, int exact, int inverted) {
    if (pri == INTERNAL_NOPRI) return inverted ? TABLE_ALLPRI : TABLE_NOPRI;
    if (pri == TABLE_ALLPRI) return inverted ? TABLE_NOPRI : TABLE_ALLPRI;

    const unsigned int bit = 1U << pri;
    const unsigned int selected = exact ? bit : (bit << 1) - 1;
    if (inverted) {
        /* A negated selector on an empty mask starts with all priorities.
         * Preserve the mask accumulated by previous selectors otherwise.
         */
        if (mask == TABLE_NOPRI) mask = TABLE_ALLPRI;
        return (uchar)(mask & ~selected);
    }
    return (uchar)(mask | selected);
}

rsRetVal DecodePRIFilter(uchar *pline, uchar pmask[]) {
    uchar buf[2048];
    assert(pline != NULL);

    for (int i = 0; i <= LOG_NFACILITIES; ++i) {
        pmask[i] = TABLE_NOPRI;
    }

    /* Scan the selectors. A dot separates each facility list from its priority. */
    for (const uchar *p = pline; *p && *p != '\t' && *p != ' ';) {
        const uchar *q;
        for (q = p; *q && *q != '\t' && *q++ != '.';) continue;

        uchar *bp;
        for (bp = buf; *q && !strchr("\t ,;", *q) && bp < buf + sizeof(buf) - 1;) *bp++ = *q++;
        *bp = '\0';
        while (*q && strchr(",;", *q)) ++q;

        const int inverted = buf[0] == '!';
        uchar *priorityName = buf + inverted;
        const int exact = *priorityName == '=';
        const int pri = decodeSyslogName(priorityName + exact, syslogPriNames);
        if (pri < 0 || (pri > LOG_DEBUG && pri != INTERNAL_NOPRI && pri != TABLE_ALLPRI)) {
            LogError(0, RS_RET_ERR, "unknown priority name \"%s\"", priorityName);
            return RS_RET_ERR;
        }

        /* Apply the same mask operation to wildcard and named facilities. */
        while (*p && !strchr("\t .;", *p)) {
            for (bp = buf; *p && !strchr("\t ,;.", *p) && bp < buf + sizeof(buf) - 1;) *bp++ = *p++;
            *bp = '\0';

            if (buf[0] == '*') {
                for (int i = 0; i <= LOG_NFACILITIES; ++i) {
                    pmask[i] = applyPrioritySelector(pmask[i], pri, exact, inverted);
                }
            } else {
                const int facility = decodeSyslogName(buf, syslogFacNames);
                if (facility < 0 || (facility >> 3) > LOG_NFACILITIES) {
                    LogError(0, RS_RET_ERR, "unknown facility name \"%s\"", buf);
                    return RS_RET_ERR;
                }
                const int i = facility >> 3;
                pmask[i] = applyPrioritySelector(pmask[i], pri, exact, inverted);
            }
            while (*p == ',' || *p == ' ') ++p;
        }
        p = q;
    }
    return RS_RET_OK;
}
