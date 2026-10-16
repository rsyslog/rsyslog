/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2007-2025 Adiscon GmbH.
 * Extracted from imtcp.c's reload preparation; see COPYING.ASL20.
 */
#ifndef INCLUDED_IMTCP_RELOAD_REGEX_H
#define INCLUDED_IMTCP_RELOAD_REGEX_H

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifdef FEATURE_REGEXP
    #include <regex.h>
#endif
#include "rsyslog.h"
#include "errmsg.h"

/** @brief Own and prevalidate a candidate pattern without runtime mutation.
 * maxLine is the effective active configuration limit, not a candidate global.
 * No session buffer is allocated here. NULL disables regex framing even for a
 * large limit; a non-NULL empty string is still a regex-enabled profile.
 */
static rsRetVal prepareReloadStartRegex(const uchar *const source, const int maxLine, uchar **const prepared) {
    DEFiRet;

    (void)maxLine;
    if (prepared == NULL || *prepared != NULL) return RS_RET_PARAM_ERROR;
    if (source == NULL) FINALIZE;
#ifdef FEATURE_REGEXP
    regex_t validationRegex;
    if (maxLine > (INT_MAX - 1) / 2) {
        LogError(0, RS_RET_ERR, "imtcp: reload framing.delimiter.regex requires maxMessageSize <= %d",
                 (INT_MAX - 1) / 2);
        ABORT_FINALIZE(RS_RET_ERR);
    }
    CHKmalloc(*prepared = (uchar *)strdup((const char *)source));
    const int errcode = regcomp(&validationRegex, (const char *)source, REG_EXTENDED);
    if (errcode != 0) {
        const rsRetVal regexRet = errcode == REG_ESPACE ? RS_RET_OUT_OF_MEMORY : RS_RET_INVALID_VALUE;
        char errbuff[512];
        regerror(errcode, &validationRegex, errbuff, sizeof(errbuff));
        LogError(0, regexRet, "imtcp: could not compile reload framing.delimiter.regex: %s", errbuff);
        ABORT_FINALIZE(regexRet);
    }
    regfree(&validationRegex);
#else
    ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
#endif

finalize_it:
    if (iRet != RS_RET_OK) {
        free(*prepared);
        *prepared = NULL;
    }
    RETiRet;
}

#endif
