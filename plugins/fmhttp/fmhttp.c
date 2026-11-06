/* SPDX-License-Identifier: Apache-2.0 */

/* fmhttp.c
 * This is a function module for http functions.
 *
 * File begun on 2018-03-16 by JGerhards
 *
 * Copyright 2010-2018 Rainer Gerhards, Jan Gerhards and Adiscon GmbH.
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
#include "rsyslog.h"
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <curl/curl.h>
#include "errmsg.h"
#include "conf.h"
#include "syslogd-types.h"
#include "template.h"
#include "msg.h"
#include "parserif.h"
#include "module-template.h"
#include "rainerscript.h"
#include "unicode-helper.h"

MODULE_TYPE_FUNCTION
MODULE_TYPE_NOKEEP;
DEF_FMOD_STATIC_DATA;

/* Fixed request limits. http_request() runs synchronously inside rule
 * evaluation, so a stalled or oversized transfer would otherwise block the
 * worker thread or exhaust memory.
 */
#define FMHTTP_CONNECT_TIMEOUT_MS 10000L
#define FMHTTP_TOTAL_TIMEOUT_MS 30000L
#define FMHTTP_MAX_REPLY_SIZE ((size_t)16 * 1024 * 1024)

struct curl_funcData {
    const char *reply;
    size_t replyLen;
    int bTooLarge;
};

/* Build a log-safe copy of a request URL: userinfo, query and fragment
 * are dropped because they may carry credentials or message content.
 * Returns NULL if out of memory.
 */
static char *redactUrlForLog(const char *const url) {
    const char *hostStart = strstr(url, "://");
    const char *const end = url + strcspn(url, "?#");
    char *redacted;
    size_t prefixLen = 0;

    if (hostStart != NULL && hostStart < end) {
        hostStart += 3;
        prefixLen = (size_t)(hostStart - url);
        const char *const authorityEnd = hostStart + strcspn(hostStart, "/?#");
        const char *const at = memchr(hostStart, '@', (size_t)(authorityEnd - hostStart));
        if (at != NULL) hostStart = at + 1;
    } else {
        hostStart = url;
    }
    const size_t restLen = (size_t)(end - hostStart);
    if ((redacted = malloc(prefixLen + restLen + 1)) == NULL) return NULL;
    memcpy(redacted, url, prefixLen);
    memcpy(redacted + prefixLen, hostStart, restLen);
    redacted[prefixLen + restLen] = '\0';
    return redacted;
}

/* Restrict a handle to plain and TLS HTTP, for requests and redirects. */
static CURLcode restrictProtocols(CURL *const handle) {
    CURLcode res;
#if LIBCURL_VERSION_NUM >= 0x075500 /* 7.85.0: string-based protocol options */
    if ((res = curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "http,https")) != CURLE_OK) return res;
    res = curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    if ((res = curl_easy_setopt(handle, CURLOPT_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS))) != CURLE_OK)
        return res;
    res = curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
    return res;
}

/* curl callback for doFunc_http_request */
static size_t curlResult(void *ptr, size_t size, size_t nmemb, void *userdata) {
    char *buf;
    size_t chunkLen;
    size_t newlen;
    struct curl_funcData *const curlData = (struct curl_funcData *)userdata;
    assert(curlData != NULL);

    if (ptr == NULL) {
        LogError(0, RS_RET_ERR, "internal error: libcurl provided ptr=NULL");
        return 0;
    }

    if (nmemb != 0 && size > SIZE_MAX / nmemb) {
        LogError(0, RS_RET_ERR, "rainerscript: http_request reply too large");
        return 0;
    }
    chunkLen = size * nmemb;
    if (curlData->replyLen > SIZE_MAX - 1 || chunkLen > SIZE_MAX - curlData->replyLen - 1 ||
        curlData->replyLen > UINT_MAX || chunkLen > UINT_MAX - curlData->replyLen) {
        LogError(0, RS_RET_ERR, "rainerscript: http_request reply too large");
        return 0;
    }
    newlen = curlData->replyLen + chunkLen;
    if (newlen > FMHTTP_MAX_REPLY_SIZE) {
        curlData->bTooLarge = 1;
        return 0; /* abort transfer, reported by the caller */
    }
    if ((buf = realloc((void *)curlData->reply, newlen + 1)) == NULL) {
        LogError(errno, RS_RET_ERR, "rainerscript: realloc failed in curlResult");
        return 0; /* abort due to failure */
    }
    memcpy(buf + curlData->replyLen, (char *)ptr, chunkLen);
    curlData->replyLen = newlen;
    curlData->reply = buf;
    return chunkLen;
}

static void ATTR_NONNULL() doFunc_http_request(struct cnffunc *__restrict__ const func,
                                               struct svar *__restrict__ const ret,
                                               void *__restrict__ const usrptr,
                                               wti_t *__restrict__ const pWti) {
    struct svar srcVal;
    int bMustFree;
    cnfexprEval(func->expr[0], &srcVal, usrptr, pWti);
    char *url = (char *)var2CString(&srcVal, &bMustFree);

    int resultSet = 0;
    CURL *handle = NULL;
    CURLcode res;
    struct curl_funcData curlData = {.reply = NULL, .replyLen = 0, .bTooLarge = 0};
    assert(func != NULL);
    rsRetVal iRet __attribute__((unused)) = RS_RET_OK;

    CHKmalloc(handle = curl_easy_init());
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curlResult);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &curlData);

    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, FMHTTP_CONNECT_TIMEOUT_MS);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, FMHTTP_TOTAL_TIMEOUT_MS);

    res = restrictProtocols(handle);
    if (res == CURLE_OK) res = curl_easy_setopt(handle, CURLOPT_URL, url);
    if (res == CURLE_OK) res = curl_easy_perform(handle);
    if (res != CURLE_OK) {
        char *const logUrl = redactUrlForLog(url);
        if (curlData.bTooLarge) {
            LogError(0, RS_RET_IO_ERROR, "rainerscript: http_request reply exceeds %zu bytes, URL: '%s'",
                     FMHTTP_MAX_REPLY_SIZE, logUrl == NULL ? "(unavailable)" : logUrl);
        } else {
            LogError(0, RS_RET_IO_ERROR, "rainerscript: http_request failed, URL: '%s', error %s",
                     logUrl == NULL ? "(unavailable)" : logUrl, curl_easy_strerror(res));
        }
        free(logUrl);
        ABORT_FINALIZE(RS_RET_OK);
    }


    CHKmalloc(ret->d.estr = es_newStrFromCStr(curlData.reply, curlData.replyLen));
    ret->datatype = 'S';
    resultSet = 1;

finalize_it:
    free((void *)curlData.reply);

    if (handle != NULL) {
        curl_easy_cleanup(handle);
    }
    if (!resultSet) {
        /* provide dummy value */
        ret->d.n = 0;
        ret->datatype = 'N';
    }
    if (bMustFree) {
        free(url);
    }
    varFreeMembers(&srcVal);
}

static rsRetVal ATTR_NONNULL(1) initFunc_http_request(struct cnffunc *const func) {
    DEFiRet;

    func->destructable_funcdata = 0;
    func->funcdata = NULL;
    if (func->nParams != 1) {
        parser_errmsg("rsyslog logic error in line %d of file %s\n", __LINE__, __FILE__);
        FINALIZE;
    }

finalize_it:
    RETiRet;
}

static void ATTR_NONNULL(1) destructFunc_http_request(struct cnffunc *const func) {
    (void)func;
}

static struct scriptFunct functions[] = {
    {"http_request", 1, 1, doFunc_http_request, initFunc_http_request, destructFunc_http_request},
    {NULL, 0, 0, NULL, NULL, NULL}  // last element to check end of array
};

BEGINgetFunctArray
    CODESTARTgetFunctArray;
    *version = 1;
    *functArray = functions;
ENDgetFunctArray


BEGINmodExit
    CODESTARTmodExit;
    curl_global_cleanup();
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_FMOD_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
    CODESTARTmodInit;
    CURLcode curlRet;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */

    curlRet = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (curlRet != CURLE_OK) {
        LogError(0, RS_RET_OBJ_CREATION_FAILED, "fmhttp: curl_global_init failed: %s", curl_easy_strerror(curlRet));
        ABORT_FINALIZE(RS_RET_OBJ_CREATION_FAILED);
    }
    CODEmodInit_QueryRegCFSLineHdlr dbgprintf("rsyslog fmhttp init called, compiled with version %s\n", VERSION);
ENDmodInit
