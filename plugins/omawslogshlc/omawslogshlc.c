/* omawslogshlc.c
 * Output module for Amazon CloudWatch Logs via the HTTP Log Collector (HLC)
 * endpoint. Uses bearer-token authentication — no AWS SDK required.
 *
 * Concurrency & Locking:
 * - Action configuration in instanceData is immutable after newActInst().
 * - Each worker owns its CURL handle and batch buffer in wrkrInstanceData_t.
 * - No mutable state is shared between workers, so no module-level lock is
 *   required for batching or posting.
 *
 * HLC endpoint reference:
 *   https://docs.aws.amazon.com/AmazonCloudWatch/latest/logs/CWL_HLC_Endpoint.html
 *
 * Copyright 2026 Adiscon GmbH.
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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <curl/curl.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("omawslogshlc")

/* HLC limits */
#define CWL_MAX_REQUEST_BYTES (1024 * 1024) /* 1 MiB max request size */
#define CWL_MAX_EVENT_BYTES (256 * 1024) /* 256 KiB max event size */
#define CWL_MAX_EVENTS_PER_REQUEST 10000

DEF_OMOD_STATIC_DATA;

typedef struct _instanceData {
    uchar *tplName;
    uchar *region;
    uchar *bearerToken;
    uchar *logGroup;
    uchar *logStream;
    int maxBatchSize; /* max events per HTTP request */
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData;
    CURL *curl;
    /* batch buffer: concatenated JSON events */
    char *batchBuf;
    size_t batchLen;
    size_t batchCap;
    size_t eventCount;
} wrkrInstanceData_t;

static struct cnfparamdescr actpdescr[] = {
    {"template", eCmdHdlrGetWord, 0}, {"region", eCmdHdlrString, 0},     {"bearer_token", eCmdHdlrString, 0},
    {"log_group", eCmdHdlrString, 0}, {"log_stream", eCmdHdlrString, 0}, {"max_batch_size", eCmdHdlrInt, 0},
};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

struct modConfData_s {
    rsconf_t *pConf;
};
static modConfData_t *runModConf = NULL;

/* ---- helpers ----------------------------------------------------------- */

static inline const char *safeStr(const uchar *s) {
    return (s == NULL) ? "<unset>" : (const char *)s;
}

/* curl write callback — discard response body (we only check HTTP status) */
static size_t discardWriteCb(void *contents, size_t size, size_t nmemb, void *userp) {
    (void)contents;
    (void)userp;
    return size * nmemb;
}

/* Return current epoch time as a double with sub-second precision. */
static double nowEpoch(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

/* URL-encode a string using the provided curl handle. Caller must curl_free(). */
static char *urlEncode(CURL *curl, const char *s) {
    if (s == NULL) return NULL;
    return curl_easy_escape(curl, s, 0);
}

/* Forward declaration for use in appendEvent */
static rsRetVal postBatch(wrkrInstanceData_t *pWrkrData);

/* ---- batch management -------------------------------------------------- */

static void resetBatch(wrkrInstanceData_t *pWrkrData) {
    pWrkrData->batchLen = 0;
    pWrkrData->eventCount = 0;
}

/* Append a single HLC event JSON object to the batch buffer.
 * Format: {"event":"<msg>","time":<epoch>,"host":"<hostname>","source":"rsyslog"}
 * The HLC endpoint accepts concatenated JSON objects (no array wrapper needed).
 */
static rsRetVal appendEvent(wrkrInstanceData_t *pWrkrData, const char *msg) {
    char hostname[256];
    const double ts = nowEpoch();
    size_t eventLen;
    size_t needed;
    DEFiRet;

    if (msg == NULL || msg[0] == '\0') {
        msg = "(empty)";
    }

    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname));
    }
    hostname[sizeof(hostname) - 1] = '\0';

    /* Build the JSON event.  We need to JSON-escape the message.
     * For simplicity we use a dynamically-sized buffer via snprintf + malloc. */
    /* First, compute the escaped message length */
    size_t msgLen = strlen(msg);
    /* Worst case: every char becomes \uXXXX (6 chars) */
    size_t escapedMaxLen = msgLen * 6 + 1;
    char *escapedMsg = malloc(escapedMaxLen);
    CHKmalloc(escapedMsg);

    {
        size_t j = 0;
        for (size_t i = 0; i < msgLen && j < escapedMaxLen - 1; i++) {
            unsigned char c = (unsigned char)msg[i];
            switch (c) {
                case '"':
                    escapedMsg[j++] = '\\';
                    escapedMsg[j++] = '"';
                    break;
                case '\\':
                    escapedMsg[j++] = '\\';
                    escapedMsg[j++] = '\\';
                    break;
                case '\b':
                    escapedMsg[j++] = '\\';
                    escapedMsg[j++] = 'b';
                    break;
                case '\f':
                    escapedMsg[j++] = '\\';
                    escapedMsg[j++] = 'f';
                    break;
                case '\n':
                    escapedMsg[j++] = '\\';
                    escapedMsg[j++] = 'n';
                    break;
                case '\r':
                    escapedMsg[j++] = '\\';
                    escapedMsg[j++] = 'r';
                    break;
                case '\t':
                    escapedMsg[j++] = '\\';
                    escapedMsg[j++] = 't';
                    break;
                default:
                    if (c < 0x20) {
                        j += snprintf(escapedMsg + j, escapedMaxLen - j, "\\u%04x", c);
                    } else {
                        escapedMsg[j++] = (char)c;
                    }
                    break;
            }
        }
        escapedMsg[j] = '\0';
    }

    /* Now build the full event JSON */
    /* {"event":"...","time":1234567890.123456,"host":"...","source":"rsyslog"} */
    size_t fullLen = strlen("{\"event\":\"\",\"time\":,\"host\":\"\",\"source\":\"rsyslog\"}") + strlen(escapedMsg) +
                     32 /* timestamp */ + strlen(hostname) + 1;
    char *fullEvent = malloc(fullLen);
    if (fullEvent == NULL) {
        free(escapedMsg);
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    int n = snprintf(fullEvent, fullLen, "{\"event\":\"%s\",\"time\":%.6f,\"host\":\"%s\",\"source\":\"rsyslog\"}",
                     escapedMsg, ts, hostname);
    free(escapedMsg);

    if (n < 0 || (size_t)n >= fullLen) {
        free(fullEvent);
        LogError(0, RS_RET_ERR, "omawslogshlc: event JSON formatting failed");
        ABORT_FINALIZE(RS_RET_ERR);
    }
    eventLen = (size_t)n;

    if (eventLen > CWL_MAX_EVENT_BYTES) {
        if (pWrkrData->eventCount > 0) {
            rsRetVal localRet = postBatch(pWrkrData);
            if (localRet != RS_RET_OK) {
                free(fullEvent);
                ABORT_FINALIZE(localRet);
            }
        }
        free(fullEvent);
        LogError(0, RS_RET_ERR, "omawslogshlc: single event size %zu exceeds 256 KiB limit", eventLen);
        ABORT_FINALIZE(RS_RET_ERR);
    }

    /* Check size limits — flush current batch if adding this event would exceed 1 MiB */
    needed = pWrkrData->batchLen + eventLen;
    if (needed > CWL_MAX_REQUEST_BYTES) {
        if (pWrkrData->eventCount > 0) {
            rsRetVal localRet = postBatch(pWrkrData);
            if (localRet != RS_RET_OK) {
                free(fullEvent);
                ABORT_FINALIZE(localRet);
            }
            needed = eventLen;
        }
        if (needed > CWL_MAX_REQUEST_BYTES) {
            free(fullEvent);
            LogError(0, RS_RET_ERR, "omawslogshlc: single event size %zu exceeds 1 MiB limit", eventLen);
            ABORT_FINALIZE(RS_RET_ERR);
        }
    }

    /* Grow buffer if needed (+1 for null terminator) */
    {
        size_t neededCap = pWrkrData->batchLen + eventLen + 1;
        if (neededCap > pWrkrData->batchCap) {
            size_t newCap = pWrkrData->batchCap * 2;
            if (newCap < neededCap) newCap = neededCap + 4096;
            char *newBuf = realloc(pWrkrData->batchBuf, newCap);
            if (newBuf == NULL) {
                free(fullEvent);
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            pWrkrData->batchBuf = newBuf;
            pWrkrData->batchCap = newCap;
        }
    }

    memcpy(pWrkrData->batchBuf + pWrkrData->batchLen, fullEvent, eventLen);
    pWrkrData->batchLen += eventLen;
    pWrkrData->eventCount++;
    free(fullEvent);

finalize_it:
    RETiRet;
}

/* ---- HTTP posting ------------------------------------------------------ */

static rsRetVal postBatch(wrkrInstanceData_t *pWrkrData) {
    instanceData *const pData = pWrkrData->pData;
    char *url = NULL;
    char *authHeader = NULL;
    char *encLogGroup = NULL;
    char *encLogStream = NULL;
    struct curl_slist *headers = NULL;
    long httpCode = 0;
    CURLcode curlRes;
    int n;
    DEFiRet;

    if (pWrkrData->eventCount == 0) {
        FINALIZE;
    }

    /* Null-terminate the batch buffer (capacity guaranteed by appendEvent) */
    pWrkrData->batchBuf[pWrkrData->batchLen] = '\0';

    /* Build URL: https://logs.<region>.amazonaws.com/services/collector/event?logGroup=<>&logStream=<> */
    encLogGroup = urlEncode(pWrkrData->curl, (const char *)pData->logGroup);
    encLogStream = urlEncode(pWrkrData->curl, (const char *)pData->logStream);
    if (encLogGroup == NULL || encLogStream == NULL) {
        LogError(0, RS_RET_ERR, "omawslogshlc: failed to URL-encode log group/stream names");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    {
        size_t urlLen = strlen("https://logs..amazonaws.com/services/collector/event?logGroup=&logStream=") +
                        strlen((const char *)pData->region) + strlen(encLogGroup) + strlen(encLogStream) + 1;
        CHKmalloc(url = malloc(urlLen));
        n = snprintf(url, urlLen, "https://logs.%s.amazonaws.com/services/collector/event?logGroup=%s&logStream=%s",
                     (const char *)pData->region, encLogGroup, encLogStream);
        if (n < 0 || (size_t)n >= urlLen) {
            LogError(0, RS_RET_ERR, "omawslogshlc: failed building HLC endpoint URL");
            ABORT_FINALIZE(RS_RET_ERR);
        }
    }

    /* Build Authorization header */
    {
        size_t authLen = strlen("Authorization: Bearer ") + strlen((const char *)pData->bearerToken) + 1;
        CHKmalloc(authHeader = malloc(authLen));
        n = snprintf(authHeader, authLen, "Authorization: Bearer %s", (const char *)pData->bearerToken);
        if (n < 0 || (size_t)n >= authLen) {
            LogError(0, RS_RET_ERR, "omawslogshlc: failed building Authorization header");
            ABORT_FINALIZE(RS_RET_ERR);
        }
    }

    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (headers == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "omawslogshlc: curl_slist_append failed");
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    headers = curl_slist_append(headers, authHeader);
    if (headers == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "omawslogshlc: curl_slist_append failed");
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    /* User-Agent: rsyslog/<version> */
    {
        char uaHeader[128];
        snprintf(uaHeader, sizeof(uaHeader), "User-Agent: rsyslog/%s", VERSION);
        headers = curl_slist_append(headers, uaHeader);
        if (headers == NULL) {
            LogError(0, RS_RET_OUT_OF_MEMORY, "omawslogshlc: curl_slist_append failed");
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
    }

    curl_easy_setopt(pWrkrData->curl, CURLOPT_URL, url);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_POSTFIELDS, pWrkrData->batchBuf);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_POSTFIELDSIZE, (long)pWrkrData->batchLen);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_WRITEFUNCTION, discardWriteCb);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_TIMEOUT, 30L);

    DBGPRINTF("omawslogshlc: posting %zu events (%zu bytes) to %s\n", pWrkrData->eventCount, pWrkrData->batchLen, url);

    curlRes = curl_easy_perform(pWrkrData->curl);
    if (curlRes != CURLE_OK) {
        LogError(0, RS_RET_SUSPENDED, "omawslogshlc: HTTP POST failed: %s", curl_easy_strerror(curlRes));
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }

    curl_easy_getinfo(pWrkrData->curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if (httpCode >= 200 && httpCode < 300) {
        DBGPRINTF("omawslogshlc: batch posted successfully, status=%ld events=%zu\n", httpCode, pWrkrData->eventCount);
        resetBatch(pWrkrData);
        FINALIZE;
    }

    /* Retryable errors */
    if (httpCode == 429 || httpCode >= 500) {
        LogError(0, RS_RET_SUSPENDED, "omawslogshlc: HTTP status=%ld (will retry)", httpCode);
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }

    /* Auth error */
    if (httpCode == 401 || httpCode == 403) {
        LogError(0, RS_RET_SUSPENDED, "omawslogshlc: HTTP status=%ld — check bearer_token configuration", httpCode);
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }

    /* Non-retryable client errors */
    LogError(0, RS_RET_ERR, "omawslogshlc: HTTP status=%ld (non-retryable)", httpCode);
    resetBatch(pWrkrData);
    ABORT_FINALIZE(RS_RET_ERR);

finalize_it:
    free(url);
    free(authHeader);
    if (encLogGroup) curl_free(encLogGroup);
    if (encLogStream) curl_free(encLogStream);
    if (headers) curl_slist_free_all(headers);
    RETiRet;
}

/* ---- rsyslog module lifecycle ------------------------------------------ */

static inline void setInstParamDefaults(instanceData *pData) {
    pData->tplName = NULL;
    pData->region = NULL;
    pData->bearerToken = NULL;
    pData->logGroup = NULL;
    pData->logStream = NULL;
    pData->maxBatchSize = 100; /* recommended by AWS docs */
}

BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    DBGPRINTF("omawslogshlc: beginCnfLoad\n");
ENDbeginCnfLoad

BEGINendCnfLoad
    CODESTARTendCnfLoad;
    DBGPRINTF("omawslogshlc: endCnfLoad\n");
ENDendCnfLoad

BEGINcheckCnf
    CODESTARTcheckCnf;
    DBGPRINTF("omawslogshlc: checkCnf\n");
ENDcheckCnf

BEGINactivateCnf
    CODESTARTactivateCnf;
    runModConf = pModConf;
    DBGPRINTF("omawslogshlc: activateCnf\n");
ENDactivateCnf

BEGINfreeCnf
    CODESTARTfreeCnf;
    DBGPRINTF("omawslogshlc: freeCnf\n");
ENDfreeCnf

BEGINcreateInstance
    CODESTARTcreateInstance;
    DBGPRINTF("omawslogshlc: createInstance[%p]\n", pData);
ENDcreateInstance

BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
    DBGPRINTF("omawslogshlc: createWrkrInstance[%p]\n", pWrkrData);
    pWrkrData->curl = curl_easy_init();
    if (pWrkrData->curl == NULL) {
        LogError(0, RS_RET_ERR, "omawslogshlc: curl_easy_init failed");
        ABORT_FINALIZE(RS_RET_ERR);
    }
    pWrkrData->batchCap = 8192;
    CHKmalloc(pWrkrData->batchBuf = malloc(pWrkrData->batchCap));
    resetBatch(pWrkrData);
finalize_it:
    if (iRet != RS_RET_OK) {
        if (pWrkrData != NULL) {
            if (pWrkrData->curl) {
                curl_easy_cleanup(pWrkrData->curl);
                pWrkrData->curl = NULL;
            }
            free(pWrkrData->batchBuf);
            pWrkrData->batchBuf = NULL;
        }
    }
ENDcreateWrkrInstance

BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURERepeatedMsgReduction) iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINfreeInstance
    CODESTARTfreeInstance;
    DBGPRINTF("omawslogshlc: freeInstance[%p]\n", pData);
    free(pData->tplName);
    free(pData->region);
    free(pData->bearerToken);
    free(pData->logGroup);
    free(pData->logStream);
ENDfreeInstance

BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
    DBGPRINTF("omawslogshlc: freeWrkrInstance[%p]\n", pWrkrData);
    /* Flush remaining events before shutdown */
    if (pWrkrData->eventCount > 0) {
        postBatch(pWrkrData);
    }
    if (pWrkrData->curl) curl_easy_cleanup(pWrkrData->curl);
    free(pWrkrData->batchBuf);
ENDfreeWrkrInstance

BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    dbgprintf("omawslogshlc\n");
    dbgprintf("\ttemplate='%s'\n", safeStr(pData->tplName));
    dbgprintf("\tregion='%s'\n", safeStr(pData->region));
    dbgprintf("\tlog_group='%s'\n", safeStr(pData->logGroup));
    dbgprintf("\tlog_stream='%s'\n", safeStr(pData->logStream));
    dbgprintf("\tmax_batch_size=%d\n", pData->maxBatchSize);
    dbgprintf("\tbearer_token=%s\n", pData->bearerToken == NULL ? "<unset>" : "<set>");
ENDdbgPrintInstInfo

BEGINtryResume
    CODESTARTtryResume;
    DBGPRINTF("omawslogshlc[%p]: tryResume\n", pWrkrData);
ENDtryResume

BEGINbeginTransaction
    CODESTARTbeginTransaction;
    DBGPRINTF("omawslogshlc[%p]: beginTransaction\n", pWrkrData);
    resetBatch(pWrkrData);
ENDbeginTransaction

BEGINdoAction
    const char *msg;
    CODESTARTdoAction;
    msg = (ppString != NULL) ? (const char *)ppString[0] : "";
    if (msg == NULL) msg = "";
    DBGPRINTF("omawslogshlc[%p]: doAction msg='%.80s%s'\n", pWrkrData, msg, strlen(msg) > 80 ? "..." : "");

    CHKiRet(appendEvent(pWrkrData, msg));

    /* Flush when batch is full */
    if ((int)pWrkrData->eventCount >= pWrkrData->pData->maxBatchSize) {
        CHKiRet(postBatch(pWrkrData));
    }

    iRet = RS_RET_DEFER_COMMIT;
finalize_it:
ENDdoAction

BEGINendTransaction
    CODESTARTendTransaction;
    DBGPRINTF("omawslogshlc[%p]: endTransaction events=%zu\n", pWrkrData, pWrkrData->eventCount);
    if (pWrkrData->eventCount > 0) {
        CHKiRet(postBatch(pWrkrData));
    }
finalize_it:
ENDendTransaction

BEGINnewActInst
    struct cnfparamvals *pvals;
    int i;
    int nTpls;
    uchar *tplToUse;
    CODESTARTnewActInst;
    DBGPRINTF("omawslogshlc: newActInst\n");

    pvals = nvlstGetParams(lst, &actpblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS, "omawslogshlc: error reading config parameters");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    CHKiRet(createInstance(&pData));
    setInstParamDefaults(pData);

    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;

        if (!strcmp(actpblk.descr[i].name, "template")) {
            free(pData->tplName);
            pData->tplName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            CHKmalloc(pData->tplName);
        } else if (!strcmp(actpblk.descr[i].name, "region")) {
            pData->region = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            CHKmalloc(pData->region);
        } else if (!strcmp(actpblk.descr[i].name, "bearer_token")) {
            pData->bearerToken = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            CHKmalloc(pData->bearerToken);
        } else if (!strcmp(actpblk.descr[i].name, "log_group")) {
            pData->logGroup = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            CHKmalloc(pData->logGroup);
        } else if (!strcmp(actpblk.descr[i].name, "log_stream")) {
            pData->logStream = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            CHKmalloc(pData->logStream);
        } else if (!strcmp(actpblk.descr[i].name, "max_batch_size")) {
            pData->maxBatchSize = (int)pvals[i].val.d.n;
        }
    }

    DBGPRINTF(
        "omawslogshlc: config region='%s' log_group='%s' log_stream='%s' "
        "max_batch_size=%d bearer_token=%s\n",
        safeStr(pData->region), safeStr(pData->logGroup), safeStr(pData->logStream), pData->maxBatchSize,
        pData->bearerToken == NULL ? "<unset>" : "<set>");

    /* Validate required parameters */
    if (pData->region == NULL || pData->region[0] == '\0') {
        LogError(0, RS_RET_PARAM_ERROR, "omawslogshlc: parameter 'region' is required");
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }
    if (pData->bearerToken == NULL || pData->bearerToken[0] == '\0') {
        LogError(0, RS_RET_PARAM_ERROR, "omawslogshlc: parameter 'bearer_token' is required");
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }
    if (pData->logGroup == NULL || pData->logGroup[0] == '\0') {
        LogError(0, RS_RET_PARAM_ERROR, "omawslogshlc: parameter 'log_group' is required");
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }
    if (pData->logStream == NULL || pData->logStream[0] == '\0') {
        LogError(0, RS_RET_PARAM_ERROR, "omawslogshlc: parameter 'log_stream' is required");
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }
    if (pData->maxBatchSize <= 0 || pData->maxBatchSize > CWL_MAX_EVENTS_PER_REQUEST) {
        LogError(0, RS_RET_PARAM_ERROR, "omawslogshlc: max_batch_size must be 1..%d, got %d",
                 CWL_MAX_EVENTS_PER_REQUEST, pData->maxBatchSize);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    nTpls = 1;
    CODE_STD_STRING_REQUESTnewActInst(nTpls);
    CHKmalloc(tplToUse =
                  (uchar *)strdup((pData->tplName == NULL) ? "RSYSLOG_TraditionalFileFormat" : (char *)pData->tplName));
    CHKiRet(OMSRsetEntry(*ppOMSR, 0, tplToUse, OMSR_NO_RQD_TPL_OPTS));

    CODE_STD_FINALIZERnewActInst;
    cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

BEGINmodExit
    CODESTARTmodExit;
    DBGPRINTF("omawslogshlc: modExit\n");
    curl_global_cleanup();
ENDmodExit

NO_LEGACY_CONF_parseSelectorAct;

BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMOD_QUERIES;
    CODEqueryEtryPt_TXIF_OMOD_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
ENDqueryEtryPt

BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION;
    CODEmodInit_QueryRegCFSLineHdlr;
    if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
        LogError(0, RS_RET_OBJ_CREATION_FAILED, "omawslogshlc: curl_global_init failed");
        ABORT_FINALIZE(RS_RET_OBJ_CREATION_FAILED);
    }
    DBGPRINTF("omawslogshlc: modInit complete\n");
ENDmodInit

/* vi:set ai:
 */
