/* SPDX-License-Identifier: Apache-2.0 */

/* omclickhouse.c
 * This is the https://clickhouse.yandex/ output module.
 *
 * Copyright 2018 Pascal Withopf and Adiscon GmbH.
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if defined(__FreeBSD__)
    #include <unistd.h>
#endif
#include <json.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "statsobj.h"
#include "cfsysline.h"
#include "unicode-helper.h"
#include "obj-types.h"
#include "ratelimit.h"
#include "ruleset.h"

#ifndef O_LARGEFILE
    #define O_LARGEFILE 0
#endif

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("omclickhouse")

/* internal structures */
DEF_OMOD_STATIC_DATA;
DEFobjCurrIf(statsobj) DEFobjCurrIf(prop) DEFobjCurrIf(ruleset)

    statsobj_t *indexStats;
STATSCOUNTER_DEF(indexSubmit, mutIndexSubmit)
STATSCOUNTER_DEF(indexHTTPFail, mutIndexHTTPFail)
STATSCOUNTER_DEF(indexHTTPReqFail, mutIndexHTTPReqFail)
STATSCOUNTER_DEF(indexFail, mutIndexFail)
STATSCOUNTER_DEF(indexSuccess, mutIndexSuccess)


typedef struct curl_slist HEADER;
typedef struct instanceConf_s {
    uchar *serverBaseUrl;
    int port;
    uchar *user;
    uchar *pwd;
    long healthCheckTimeout;
    long timeout;
    uchar *authBuf;
    uchar *tplName;
    sbool useHttps;
    sbool allowUnsignedCerts;
    sbool skipVerifyHost;
    int fdErrFile;
    uchar *errorFile;
    sbool bulkmode;
    size_t maxbytes;
    int maxConsecutiveRejects; /* 0: never stop dropping rejected rows */
    uchar *caCertFile;
    uchar *myCertFile;
    uchar *myPrivKeyFile;
    struct instanceConf_s *next;
} instanceData;

struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
    instanceConf_t *root, *tail;
};
static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */

typedef struct wrkrInstanceData {
    PTR_ASSERT_DEF
    instanceData *pData;
    CURL *curlPostHandle; /* libcurl session handle for posting data to the server */
    HEADER *curlHeader; /* json POST request info */
    CURL *curlCheckConnHandle; /* libcurl session handle for checking the server connection */
    int replyLen;
    char *reply;
    long exceptionCode; /* X-ClickHouse-Exception-Code of the last insert, -1 if none */
    int rejectsInRow; /* final rejections since the last accepted insert */
    int insertFailures; /* inserts ClickHouse answered with a failure, in a row */
    sbool backoffPending; /* the next tryResume() reports SUSPENDED */
    sbool rejectLimitLogged;
    uchar *restURL;
    struct {
        es_str_t *data;
        int nmemb; /* number of messages in batch (for statistics counting) */
        size_t *rowOff; /* start of each message's part in data */
        int rowCap; /* allocated entries in rowOff */
        size_t valuesPrefix; /* length of "INSERT ... VALUES " in the first row, 0 if none */
        sbool heldInTx; /* an early flush was suspended; its rows stay, no more early flushes */
        sbool failedInTx; /* a local error; endTransaction() suspends the transaction */
    } batch;
} wrkrInstanceData_t;

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {{"server", eCmdHdlrGetWord, 0},
                                           {"port", eCmdHdlrInt, 0},
                                           {"user", eCmdHdlrGetWord, 0},
                                           {"pwd", eCmdHdlrGetWord, 0},
                                           {"healthchecktimeout", eCmdHdlrInt, 0},
                                           {"timeout", eCmdHdlrInt, 0},
                                           {"template", eCmdHdlrGetWord, 0},
                                           {"usehttps", eCmdHdlrBinary, 0},
                                           {"allowunsignedcerts", eCmdHdlrBinary, 0},
                                           {"skipverifyhost", eCmdHdlrBinary, 0},
                                           {"errorfile", eCmdHdlrGetWord, 0},
                                           {"bulkmode", eCmdHdlrBinary, 0},
                                           {"maxbytes", eCmdHdlrSize, 0},
                                           {"maxconsecutiverejects", eCmdHdlrNonNegInt, 0},
                                           {"tls.cacert", eCmdHdlrString, 0},
                                           {"tls.mycert", eCmdHdlrString, 0},
                                           {"tls.myprivkey", eCmdHdlrString, 0}};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

static rsRetVal curlSetup(wrkrInstanceData_t *pWrkrData);

BEGINcreateInstance
    CODESTARTcreateInstance;
    pData->fdErrFile = -1;
    pData->caCertFile = NULL;
    pData->myCertFile = NULL;
    pData->myPrivKeyFile = NULL;
ENDcreateInstance

BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
    pWrkrData->curlHeader = NULL;
    pWrkrData->curlPostHandle = NULL;
    pWrkrData->curlCheckConnHandle = NULL;
    pWrkrData->restURL = NULL;
    if (pData->bulkmode) {
        if ((pWrkrData->batch.data = es_newStr(1024)) == NULL) {
            LogError(0, RS_RET_OUT_OF_MEMORY,
                     "omclickhouse: error creating batch string "
                     "turned off bulk mode\n");
            pData->bulkmode = 0; /* at least it works */
        }
    }
    pWrkrData->batch.rowOff = NULL;
    pWrkrData->batch.rowCap = 0;
    pWrkrData->batch.valuesPrefix = 0;
    pWrkrData->batch.heldInTx = 0;
    pWrkrData->batch.failedInTx = 0;
    pWrkrData->exceptionCode = -1;
    pWrkrData->rejectsInRow = 0;
    pWrkrData->insertFailures = 0;
    pWrkrData->backoffPending = 0;
    pWrkrData->rejectLimitLogged = 0;

    iRet = curlSetup(pWrkrData);
ENDcreateWrkrInstance

BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURERepeatedMsgReduction) iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINfreeInstance
    CODESTARTfreeInstance;
    free(pData->serverBaseUrl);
    free(pData->user);
    free(pData->pwd);
    free(pData->authBuf);
    if (pData->fdErrFile != -1) close(pData->fdErrFile);
    free(pData->errorFile);
    free(pData->tplName);
    free(pData->caCertFile);
    free(pData->myCertFile);
    free(pData->myPrivKeyFile);
ENDfreeInstance

BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
    if (pWrkrData->curlHeader != NULL) {
        curl_slist_free_all(pWrkrData->curlHeader);
        pWrkrData->curlHeader = NULL;
    }
    if (pWrkrData->curlCheckConnHandle != NULL) {
        curl_easy_cleanup(pWrkrData->curlCheckConnHandle);
        pWrkrData->curlCheckConnHandle = NULL;
    }
    if (pWrkrData->curlPostHandle != NULL) {
        curl_easy_cleanup(pWrkrData->curlPostHandle);
        pWrkrData->curlPostHandle = NULL;
    }
    if (pWrkrData->restURL != NULL) {
        free(pWrkrData->restURL);
        pWrkrData->restURL = NULL;
    }
    es_deleteStr(pWrkrData->batch.data);
    free(pWrkrData->batch.rowOff);
ENDfreeWrkrInstance

BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    dbgprintf("omclickhouse\n");
    dbgprintf("\tserver='%s'\n", pData->serverBaseUrl);
    dbgprintf("\tport='%d'\n", pData->port);
    dbgprintf("\tuser='%s'\n", pData->user);
    dbgprintf("\tpwd='%s'\n", pData->pwd);
    dbgprintf("\thealthCheckTimeout=%lu\n", pData->healthCheckTimeout);
    dbgprintf("\ttimeout=%lu\n", pData->timeout);
    dbgprintf("\ttemplate='%s'\n", pData->tplName);
    dbgprintf("\tusehttps='%d'\n", pData->useHttps);
    dbgprintf("\tallowunsignedcerts='%d'\n", pData->allowUnsignedCerts);
    dbgprintf("\tskipverifyhost='%d'\n", pData->skipVerifyHost);
    dbgprintf("\terrorFile='%s'\n", pData->errorFile);
    dbgprintf("\tbulkmode='%d'\n", pData->bulkmode);
    dbgprintf("\tmaxbytes='%zu'\n", pData->maxbytes);
    dbgprintf("\tmaxconsecutiverejects=%d\n", pData->maxConsecutiveRejects);
    dbgprintf("\ttls.cacert='%s'\n", pData->caCertFile);
    dbgprintf("\ttls.mycert='%s'\n", pData->myCertFile);
    dbgprintf("\ttls.myprivkey='%s'\n", pData->myPrivKeyFile);
ENDdbgPrintInstInfo


/* checks if connection to clickhouse can be established
 */
static rsRetVal ATTR_NONNULL() checkConn(wrkrInstanceData_t *const pWrkrData) {
    CURL *curl;
    CURLcode res;
    char errbuf[CURL_ERROR_SIZE] = "";
    const char *healthCheckMessage = "SELECT 1";
    DEFiRet;

    pWrkrData->reply = NULL;
    pWrkrData->replyLen = 0;
    curl = pWrkrData->curlCheckConnHandle;


    curl_easy_setopt(curl, CURLOPT_URL, pWrkrData->restURL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, healthCheckMessage);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(healthCheckMessage));
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        DBGPRINTF("omclickhouse: checkConn completed with success\n");
        ABORT_FINALIZE(RS_RET_OK);
    }

    LogMsg(0, RS_RET_SUSPENDED, LOG_WARNING, "omclickhouse: checkConn failed: %s",
           errbuf[0] != '\0' ? errbuf : curl_easy_strerror(res));
    ABORT_FINALIZE(RS_RET_SUSPENDED);

finalize_it:
    free(pWrkrData->reply);
    pWrkrData->reply = NULL; /* don't leave dangling pointer */
    RETiRet;
}


BEGINtryResume
    CODESTARTtryResume;
    dbgprintf("omclickhouse: tryResume called\n");
    /* SELECT 1 succeeds while ClickHouse keeps failing the insert itself (a
     * missing table, missing grants, overload). Resuming at once would post
     * the batch again without a pause: the engine skips its retry sleep with
     * action.resumeRetryCount="0", and every rebuilt batch resets its count
     * of quick resumes. So once two inserts in a row failed, report one more
     * suspension and let the engine apply action.resumeInterval before the
     * next check.
     */
    if (pWrkrData->backoffPending) {
        pWrkrData->backoffPending = 0;
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }
    iRet = checkConn(pWrkrData);
finalize_it:
ENDtryResume


/*
 * Render a rejected request as one JSON object: URL and body of the request,
 * HTTP status, and ClickHouse reply. The action drops data that ClickHouse
 * rejects, so this record is the only copy of it that is kept.
 */
static rsRetVal getDataErrorDefault(
    wrkrInstanceData_t *pWrkrData, const char *reply, const uchar *reqmsg, const long httpStatus, char **rendered) {
    DEFiRet;
    fjson_object *req = NULL;
    fjson_object *errRoot = NULL;

    if ((req = fjson_object_new_object()) == NULL) ABORT_FINALIZE(RS_RET_ERR);
    /* restURL is unset when the first row of a worker is rejected before it is
     * posted (a VALUES row that is not an INSERT).
     */
    fjson_object_object_add(req, "url",
                            fjson_object_new_string(pWrkrData->restURL == NULL ? "" : (char *)pWrkrData->restURL));
    fjson_object_object_add(req, "postdata", fjson_object_new_string((const char *)reqmsg));

    if ((errRoot = fjson_object_new_object()) == NULL) ABORT_FINALIZE(RS_RET_ERR);
    fjson_object_object_add(errRoot, "request", req);
    fjson_object_object_add(errRoot, "http_status", fjson_object_new_int64(httpStatus));
    fjson_object_object_add(errRoot, "reply", fjson_object_new_string(reply));
    *rendered = strdup((char *)fjson_object_to_json_string(errRoot));
    if (*rendered == NULL) {
        fjson_object_put(errRoot);
        req = NULL;
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    req = NULL;
    fjson_object_put(errRoot);

finalize_it:
    fjson_object_put(req);
    RETiRet;
}


/* write data error request/replies to separate error file
 * Note: we open the file but never close it before exit. If it
 * needs to be closed, HUP must be sent.
 */
static rsRetVal ATTR_NONNULL() writeDataError(wrkrInstanceData_t *const pWrkrData,
                                              const uchar *const reqmsg,
                                              const long httpStatus,
                                              const char *const reply) {
    DEFiRet;
    instanceData *pData = pWrkrData->pData;
    char *rendered = NULL;
    size_t toWrite;
    ssize_t wrRet;

    if (pData->errorFile == NULL) {
        dbgprintf(
            "omclickhouse: no local error logger defined - "
            "ignoring ClickHouse error information\n");
        FINALIZE;
    }


    if (pData->fdErrFile == -1) {
        pData->fdErrFile = open((char *)pData->errorFile, O_WRONLY | O_CREAT | O_APPEND | O_LARGEFILE | O_CLOEXEC,
                                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (pData->fdErrFile == -1) {
            LogError(errno, RS_RET_ERR, "omclickhouse: error opening error file %s", pData->errorFile);
            ABORT_FINALIZE(RS_RET_ERR);
        }
    }

    if (getDataErrorDefault(pWrkrData, reply, reqmsg, httpStatus, &rendered) != RS_RET_OK) {
        ABORT_FINALIZE(RS_RET_ERR);
    }

    dbgprintf("omclickhouse: message sent: '%s'\n", reqmsg);
    dbgprintf("omclickhouse: error record: '%s'\n", rendered);
    toWrite = strlen(rendered) + 1;
    /* Note: we overwrite the '\0' terminator with '\n' -- so we avoid
     * caling malloc() -- write() does NOT need '\0'!
     */
    rendered[toWrite - 1] = '\n'; /* NO LONGER A STRING! */
    wrRet = write(pData->fdErrFile, rendered, toWrite);
    if (wrRet != (ssize_t)toWrite) {
        LogError(errno, RS_RET_IO_ERROR, "omclickhouse: error writing error file %s, write returned %lld",
                 pData->errorFile, (long long)wrRet);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }

finalize_it:
    free(rendered);
    RETiRet;
}


/* ClickHouse error codes caused by the content of a row. ClickHouse sends them
 * with HTTP 400, 404 (UNKNOWN_IDENTIFIER for a bad token in a VALUES row) or
 * 500 (VIOLATED_CONSTRAINT), so the code decides, not the status. Names are
 * from errorCodeToName() in ClickHouse 26.8. CANNOT_READ_ALL_DATA is left out:
 * a body cut short in transit is not bad data.
 */
static int isDataErrorCode(const long code) {
    switch (code) {
        case 6: /* CANNOT_PARSE_TEXT */
        case 25: /* CANNOT_PARSE_ESCAPE_SEQUENCE */
        case 26: /* CANNOT_PARSE_QUOTED_STRING */
        case 27: /* CANNOT_PARSE_INPUT_ASSERTION_FAILED */
        case 32: /* ATTEMPT_TO_READ_AFTER_EOF */
        case 38: /* CANNOT_PARSE_DATE */
        case 41: /* CANNOT_PARSE_DATETIME */
        case 47: /* UNKNOWN_IDENTIFIER */
        case 53: /* TYPE_MISMATCH */
        case 62: /* SYNTAX_ERROR */
        case 70: /* CANNOT_CONVERT_TYPE */
        case 72: /* CANNOT_PARSE_NUMBER */
        case 117: /* INCORRECT_DATA */
        case 128: /* TOO_LARGE_ARRAY_SIZE */
        case 130: /* CANNOT_READ_ARRAY_FROM_TEXT */
        case 131: /* TOO_LARGE_STRING_SIZE */
        case 190: /* SIZES_OF_ARRAYS_DONT_MATCH */
        case 321: /* VALUE_IS_OUT_OF_RANGE_OF_DATA_TYPE */
        case 349: /* CANNOT_INSERT_NULL_IN_ORDINARY_COLUMN */
        case 376: /* CANNOT_PARSE_UUID */
        case 441: /* CANNOT_PARSE_DOMAIN_VALUE_FROM_STRING */
        case 467: /* CANNOT_PARSE_BOOL */
        case 469: /* VIOLATED_CONSTRAINT */
        case 675: /* CANNOT_PARSE_IPV4 */
        case 676: /* CANNOT_PARSE_IPV6 */
            return 1;
        default:
            return 0;
    }
}

/* ClickHouse exception code of the last reply: the X-ClickHouse-Exception-Code
 * header, else the "Code: N." that starts the body. -1 if there is neither.
 */
static long replyExceptionCode(const wrkrInstanceData_t *const pWrkrData, const char *const reply) {
    char *end = NULL;
    long code;

    if (pWrkrData->exceptionCode >= 0) return pWrkrData->exceptionCode;
    if (strncmp(reply, "Code: ", 6) != 0) return -1;
    errno = 0;
    code = strtol(reply + 6, &end, 10);
    if (errno != 0 || end == reply + 6 || *end != '.' || code < 0) return -1;
    return code;
}

/* ClickHouse answered an insert with a failure that suspends the action. The
 * engine retries once at once, which keeps a single failure cheap. A second one
 * in a row turns on the backoff in tryResume().
 */
static void ATTR_NONNULL() noteInsertFailed(wrkrInstanceData_t *const pWrkrData) {
    if (pWrkrData->insertFailures < INT_MAX) ++pWrkrData->insertFailures;
    if (pWrkrData->insertFailures >= 2) pWrkrData->backoffPending = 1;
}


/* A row rejected as bad data, by ClickHouse (httpStatus > 0) or by the module
 * itself (httpStatus 0). For a request of several messages nothing is
 * recorded: the caller resends each row on its own. A final rejection is
 * written to the error file and dropped. It is kept instead when the error
 * file cannot be written, or when maxConsecutiveRejects rows in a row were
 * rejected; RS_RET_SUSPENDED then keeps the row queued.
 */
static rsRetVal rejectData(wrkrInstanceData_t *pWrkrData,
                           const uchar *const reqmsg,
                           const long httpStatus,
                           const char *const reply,
                           const int nmsgs,
                           const sbool final) {
    const int limit = pWrkrData->pData->maxConsecutiveRejects;
    DEFiRet;

    if (!final) {
        LogMsg(0, RS_RET_DATAFAIL, LOG_WARNING,
               "omclickhouse: a batch of %d messages was rejected with HTTP status %ld, "
               "retrying them one at a time: %s",
               nmsgs, httpStatus, reply);
        ABORT_FINALIZE(RS_RET_DATAFAIL);
    }
    if (limit > 0 && pWrkrData->rejectsInRow >= limit) {
        if (!pWrkrData->rejectLimitLogged) {
            LogError(0, RS_RET_SUSPENDED,
                     "omclickhouse: %d rows in a row were rejected, which points to a schema or template "
                     "problem rather than bad data; keeping rows queued until ClickHouse accepts them: %s",
                     limit, reply);
            pWrkrData->rejectLimitLogged = 1;
        }
        noteInsertFailed(pWrkrData);
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }
    if (pWrkrData->pData->errorFile != NULL && writeDataError(pWrkrData, reqmsg, httpStatus, reply) != RS_RET_OK) {
        LogError(0, RS_RET_SUSPENDED,
                 "omclickhouse: could not record a rejected row in the error file, keeping it queued: %s", reply);
        noteInsertFailed(pWrkrData);
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }
    ++pWrkrData->rejectsInRow;
    if (httpStatus > 0) {
        LogError(0, RS_RET_DATAFAIL, "omclickhouse: ClickHouse rejected the data with HTTP status %ld: %s", httpStatus,
                 reply);
    } else {
        LogError(0, RS_RET_DATAFAIL, "omclickhouse: dropping a row: %s", reply);
    }
    ABORT_FINALIZE(RS_RET_DATAFAIL);

finalize_it:
    RETiRet;
}


/* Case-insensitive search for "INSERT INTO". */
static int hasInsertInto(const char *msg) {
    static const char word[] = "INSERT INTO";
    for (; *msg != '\0'; ++msg) {
        if (strncasecmp(msg, word, sizeof(word) - 1) == 0) return 1;
    }
    return 0;
}


/* Note an accepted insert: the run of rejected rows is over. */
static void ATTR_NONNULL() noteAccepted(wrkrInstanceData_t *const pWrkrData) {
    if (pWrkrData->rejectLimitLogged) {
        LogMsg(0, RS_RET_OK, LOG_INFO, "omclickhouse: ClickHouse accepts rows again, dropping rejected rows resumes");
    }
    pWrkrData->rejectsInRow = 0;
    pWrkrData->rejectLimitLogged = 0;
    pWrkrData->insertFailures = 0;
    pWrkrData->backoffPending = 0;
}

/* Classify a reply that is not a plain success.
 *
 * A data error is a ClickHouse exception code from isDataErrorCode(), or,
 * when the reply carries no code, HTTP 400 or 413. It returns RS_RET_DATAFAIL.
 * When the request held several messages, nothing is recorded yet:
 * postRowsOneByOne() resends each message on its own and only those that are
 * still rejected are dropped. A final rejection (one message) goes to the
 * error file.
 *
 * After maxConsecutiveRejects final rejections with no accepted insert between
 * them, a further rejection suspends the action instead of dropping the row.
 * A schema change or a broken template rejects every row, and those rows are
 * then kept in the queue until ClickHouse accepts them again.
 *
 * Every other failure suspends the action. Authentication failures, a missing
 * table or column, overload, and server errors are not caused by the data, so
 * the batch stays queued and is retried.
 */
static rsRetVal checkResult(
    wrkrInstanceData_t *pWrkrData, const uchar *reqmsg, const long httpStatus, const int nmsgs, const sbool final) {
    const char *const reply = pWrkrData->reply == NULL ? "" : pWrkrData->reply;
    const long code = replyExceptionCode(pWrkrData, reply);
    int dataError;
    DEFiRet;

    if (httpStatus < 400 && code < 0 && strstr(reply, "DB::Exception") == NULL &&
        strstr(reply, "DB::NetException") == NULL) {
        noteAccepted(pWrkrData);
        FINALIZE;
    }

    dbgprintf("omclickhouse: action failed with HTTP status %ld, code %ld and reply: %s\n", httpStatus, code, reply);
    STATSCOUNTER_INC(indexFail, mutIndexFail);
    dataError = (code >= 0) ? isDataErrorCode(code) : (httpStatus == 400 || httpStatus == 413);
    if (!dataError) {
        LogError(0, RS_RET_SUSPENDED, "omclickhouse: ClickHouse request failed with HTTP status %ld: %s", httpStatus,
                 reply);
        noteInsertFailed(pWrkrData);
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }
    ABORT_FINALIZE(rejectData(pWrkrData, reqmsg, httpStatus, reply, nmsgs, final));

finalize_it:
    RETiRet;
}

static rsRetVal ATTR_NONNULL(1) setPostURL(wrkrInstanceData_t *const pWrkrData) {
    char *baseUrl;
    es_str_t *url;
    DEFiRet;
    instanceData *const pData = pWrkrData->pData;

    baseUrl = (char *)pData->serverBaseUrl;
    url = es_newStrFromCStr(baseUrl, strlen(baseUrl));
    if (url == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "omclickhouse: error allocating new estr for POST url.");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    if (pWrkrData->restURL != NULL) free(pWrkrData->restURL);

    pWrkrData->restURL = (uchar *)es_str2cstr(url, NULL);
    curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_URL, pWrkrData->restURL);
    dbgprintf("omclickhouse: using REST URL: '%s'\n", pWrkrData->restURL);

finalize_it:
    if (url != NULL) es_deleteStr(url);
    RETiRet;
}


/* this method computes the next data set to be added to the batch
 * returns the expected size of adding the next message into the
 * batched request to clickhouse
 */
static size_t computeBulkMessage(const wrkrInstanceData_t *const pWrkrData,
                                 const uchar *const message,
                                 const char **newMessage) {
    size_t r = 0;
    const char *v;
    /* A later row is cut to its "(...)" part only when the first row gave
     * the statement in front of it, so that postRowsOneByOne() can rebuild a
     * whole statement for every row.
     */
    if (pWrkrData->batch.nmemb != 0 && pWrkrData->batch.valuesPrefix > 0 &&
        (v = strstr((const char *)message, "VALUES")) != NULL && (v = strchr(v, '(')) != NULL) {
        *newMessage = v;
        r = strlen(*newMessage);
    } else {
        *newMessage = (const char *)message;
        r = strlen(*newMessage);
    }
    dbgprintf("omclickhouse: computeBulkMessage: new message part: %s\n", *newMessage);

    return r;
}


/* This method builds the batch, that will be submitted.
 */
static rsRetVal buildBatch(wrkrInstanceData_t *pWrkrData, const char *message) {
    DEFiRet;
    int length = strlen(message);
    size_t start;
    int r;

    if (pWrkrData->batch.nmemb >= pWrkrData->batch.rowCap) {
        const int cap = pWrkrData->batch.rowCap == 0 ? 64 : 2 * pWrkrData->batch.rowCap;
        size_t *const rows = realloc(pWrkrData->batch.rowOff, sizeof(size_t) * (size_t)cap);
        CHKmalloc(rows);
        pWrkrData->batch.rowOff = rows;
        pWrkrData->batch.rowCap = cap;
    }
    start = es_strlen(pWrkrData->batch.data);
    if (pWrkrData->batch.nmemb == 0) {
        /* Later rows hold only their "(...)" part. Keep the length of the
         * statement before the first one, to resend a row on its own.
         */
        const char *v = strstr(message, "VALUES");
        if (v != NULL && (v = strchr(v, '(')) != NULL) pWrkrData->batch.valuesPrefix = (size_t)(v - message);
    }
    r = es_addBuf(&pWrkrData->batch.data, message, length);
    if (r != 0) {
        LogError(0, RS_RET_ERR, "omclickhouse: growing batch failed with code %d", r);
        ABORT_FINALIZE(RS_RET_ERR);
    }
    pWrkrData->batch.rowOff[pWrkrData->batch.nmemb] = start;
    ++pWrkrData->batch.nmemb;
    iRet = RS_RET_OK;

finalize_it:
    RETiRet;
}


static void ATTR_NONNULL() initializeBatch(wrkrInstanceData_t *pWrkrData) {
    es_emptyStr(pWrkrData->batch.data);
    pWrkrData->batch.nmemb = 0;
    pWrkrData->batch.valuesPrefix = 0;
}


static rsRetVal ATTR_NONNULL(1, 2)
    curlPost(wrkrInstanceData_t *pWrkrData, uchar *message, int msglen, const int nmsgs, const sbool final) {
    CURLcode code;
    CURL *const curl = pWrkrData->curlPostHandle;
    char errbuf[CURL_ERROR_SIZE] = "";
    long httpStatus = 0;
    DEFiRet;

    if (!hasInsertInto((char *)message)) {
        indexHTTPFail += nmsgs;
        ABORT_FINALIZE(rejectData(pWrkrData, message, 0, "message is not an INSERT query", nmsgs, final));
    }

    pWrkrData->reply = NULL;
    pWrkrData->replyLen = 0;
    pWrkrData->exceptionCode = -1;

    CHKiRet(setPostURL(pWrkrData));

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (char *)message);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)msglen);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
    dbgprintf("curl returned %lld\n", (long long)code);
    if (code != CURLE_OK && code != CURLE_HTTP_RETURNED_ERROR) {
        STATSCOUNTER_INC(indexHTTPReqFail, mutIndexHTTPReqFail);
        indexHTTPFail += nmsgs;
        LogError(0, RS_RET_SUSPENDED,
                 "omclickhouse: we are suspending ourselfs due "
                 "to server failure %lld: %s",
                 (long long)code, errbuf);
        /* SELECT 1 may still succeed while the insert connection is dropped */
        noteInsertFailed(pWrkrData);
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }

    if (pWrkrData->reply == NULL && httpStatus < 400 && pWrkrData->exceptionCode < 0) {
        dbgprintf("omclickhouse: pWrkrData reply==NULL, replyLen = '%d'\n", pWrkrData->replyLen);
        STATSCOUNTER_INC(indexSuccess, mutIndexSuccess);
        noteAccepted(pWrkrData);
    } else {
        dbgprintf("omclickhouse: pWrkrData replyLen = '%d'\n", pWrkrData->replyLen);
        if (pWrkrData->reply != NULL && pWrkrData->replyLen > 0) {
            pWrkrData->reply[pWrkrData->replyLen] = '\0';
            /* Append 0 Byte if replyLen is above 0 - byte has been reserved in malloc */
        }
        dbgprintf("omclickhouse: pWrkrData reply: '%s'\n", pWrkrData->reply == NULL ? "" : pWrkrData->reply);
        CHKiRet(checkResult(pWrkrData, message, httpStatus, nmsgs, final));
    }

finalize_it:
    free(pWrkrData->reply);
    pWrkrData->reply = NULL; /* don't leave dangling pointer */
    RETiRet;
}


/* ClickHouse rejected a batch as bad data. It inserts all rows of a request or
 * none, so each row is sent again on its own: accepted rows are inserted, and
 * rows that are still rejected are recorded and dropped. Leaving the split to
 * the action engine through RS_RET_DATAFAIL is not safe: the engine's retry
 * loop for a transaction that was suspended before does not handle that code
 * and posts the batch again without a pause.
 */
static rsRetVal postRowsOneByOne(wrkrInstanceData_t *pWrkrData,
                                 const char *const batch,
                                 const size_t len,
                                 int *const handled) {
    const size_t prefix = pWrkrData->batch.valuesPrefix;
    char *row = NULL;
    int i;
    DEFiRet;

    *handled = 0;
    for (i = 0; i < pWrkrData->batch.nmemb; ++i) {
        const size_t start = pWrkrData->batch.rowOff[i];
        const size_t end = (i + 1 < pWrkrData->batch.nmemb) ? pWrkrData->batch.rowOff[i + 1] : len;
        const size_t rowLen = end - start;
        const size_t head = (i > 0 && prefix > 0 && batch[start] == '(') ? prefix : 0;
        rsRetVal rowRet;

        free(row);
        CHKmalloc(row = malloc(head + rowLen + 1));
        memcpy(row, batch, head);
        memcpy(row + head, batch + start, rowLen);
        row[head + rowLen] = '\0';
        rowRet = curlPost(pWrkrData, (uchar *)row, (int)(head + rowLen), 1, 1);
        if (rowRet != RS_RET_OK && rowRet != RS_RET_DATAFAIL) {
            ABORT_FINALIZE(rowRet);
        }
        *handled = i + 1;
    }

finalize_it:
    free(row);
    RETiRet;
}


/* A held batch keeps only the rows from "first" on: the rows before it were
 * inserted or recorded while it was resent one row at a time. Without this,
 * the next post would insert those rows again and record a rejected row twice.
 * An allocation failure is returned: doAction() then treats it as a local
 * error and endTransaction() suspends the whole transaction.
 */
static rsRetVal keepRowsFrom(wrkrInstanceData_t *pWrkrData,
                             const char *const batch,
                             const size_t len,
                             const int first) {
    const size_t start = pWrkrData->batch.rowOff[first];
    const size_t prefix = pWrkrData->batch.valuesPrefix;
    const size_t head = (prefix > 0 && batch[start] == '(') ? prefix : 0;
    es_str_t *data;
    int k;

    if ((data = es_newStr((es_size_t)(head + len - start + 1))) == NULL) return RS_RET_OUT_OF_MEMORY;
    if ((head > 0 && es_addBuf(&data, (char *)batch, (es_size_t)head) != 0) ||
        es_addBuf(&data, (char *)batch + start, (es_size_t)(len - start)) != 0) {
        es_deleteStr(data);
        return RS_RET_OUT_OF_MEMORY;
    }
    for (k = 0; k + first < pWrkrData->batch.nmemb; ++k) {
        pWrkrData->batch.rowOff[k] = (k == 0) ? 0 : pWrkrData->batch.rowOff[k + first] - start + head;
    }
    pWrkrData->batch.nmemb -= first;
    es_deleteStr(pWrkrData->batch.data);
    pWrkrData->batch.data = data;
    return RS_RET_OK;
}


/* Post the batch. Returns RS_RET_OK when every row was inserted or recorded
 * as rejected, RS_RET_SUSPENDED to retry, or a local error.
 * keepOnSuspend: leave the rows in the batch when the post is suspended, so a
 * later post sends them again.
 */
static rsRetVal submitBatch(wrkrInstanceData_t *pWrkrData, const sbool keepOnSuspend) {
    char *cstr = NULL;
    size_t len = 0;
    int handled = 0;
    DEFiRet;

    CHKmalloc(cstr = es_str2cstr(pWrkrData->batch.data, NULL));
    len = strlen(cstr);
    dbgprintf("omclickhouse: submitBatch, batch: '%s'\n", cstr);

    iRet = curlPost(pWrkrData, (uchar *)cstr, (int)len, pWrkrData->batch.nmemb, pWrkrData->batch.nmemb == 1);
    if (iRet == RS_RET_DATAFAIL && pWrkrData->batch.nmemb > 1) {
        iRet = postRowsOneByOne(pWrkrData, cstr, len, &handled);
    }
    /* A final rejection is recorded and dropped; nothing is left to retry. */
    if (iRet == RS_RET_DATAFAIL) iRet = RS_RET_OK;

finalize_it:
    if (!(keepOnSuspend && iRet == RS_RET_SUSPENDED)) {
        initializeBatch(pWrkrData);
    } else if (handled > 0 && keepRowsFrom(pWrkrData, cstr, len, handled) != RS_RET_OK) {
        iRet = RS_RET_OUT_OF_MEMORY;
    }
    free(cstr);
    RETiRet;
}

BEGINbeginTransaction
    CODESTARTbeginTransaction;
    if (!pWrkrData->pData->bulkmode) {
        FINALIZE;
    }

    initializeBatch(pWrkrData);
    pWrkrData->batch.heldInTx = 0;
    pWrkrData->batch.failedInTx = 0;
finalize_it:
ENDbeginTransaction


BEGINdoAction
    const char *batchPart = NULL;
    CODESTARTdoAction;
    dbgprintf("CODESTARTdoAction: entered\n");
    STATSCOUNTER_INC(indexSubmit, mutIndexSubmit);

    if (pWrkrData->pData->bulkmode) {
        if (pWrkrData->batch.failedInTx) {
            /* endTransaction() suspends this transaction; the engine resends
             * all of its messages.
             */
            ABORT_FINALIZE(RS_RET_DEFER_COMMIT);
        }
        const size_t nBytes = computeBulkMessage(pWrkrData, ppString[0], &batchPart);
        dbgprintf("pascal: doAction: message: %s\n", batchPart);

        /* If max bytes is set and this next message will put us over the limit,
         * submit the current buffer and reset */
        if (pWrkrData->pData->maxbytes > 0 && pWrkrData->batch.nmemb > 0 && !pWrkrData->batch.heldInTx &&
            es_strlen(pWrkrData->batch.data) + nBytes > pWrkrData->pData->maxbytes) {
            dbgprintf(
                "omclickhouse: maxbytes limit reached, submitting partial "
                "batch of %d elements.\n",
                pWrkrData->batch.nmemb);
            /* A rejected row is handled inside submitBatch(). A suspended
             * flush keeps its rows, and endTransaction() posts them with the
             * rest: RS_RET_SUSPENDED from here would make the engine retry
             * only this message after beginTransaction() cleared the batch.
             */
            const rsRetVal flushRet = submitBatch(pWrkrData, 1);
            if (flushRet == RS_RET_OK) {
                batchPart = (const char *)ppString[0];
            } else if (flushRet == RS_RET_SUSPENDED) {
                pWrkrData->batch.heldInTx = 1;
            } else {
                ABORT_FINALIZE(flushRet);
            }
        }

        CHKiRet(buildBatch(pWrkrData, batchPart));

        iRet = pWrkrData->batch.nmemb == 1 ? RS_RET_PREVIOUS_COMMITTED : RS_RET_DEFER_COMMIT;
    } else {
        /* One message per request: a rejection is final and already recorded.
         * RS_RET_DATAFAIL from doAction() would leave the worker dropping
         * later messages, so the failure is consumed here.
         */
        const rsRetVal postRet = curlPost(pWrkrData, ppString[0], strlen((char *)ppString[0]), 1, 1);
        if (postRet != RS_RET_DATAFAIL) {
            CHKiRet(postRet);
        }
    }
finalize_it:
    /* The engine handles only these codes without dropping messages. Anything
     * else is a local failure such as an allocation error. In bulk mode the
     * whole transaction is suspended at endTransaction(), so the rows already
     * batched are resent; a single message is suspended here.
     */
    if (iRet != RS_RET_OK && iRet != RS_RET_DEFER_COMMIT && iRet != RS_RET_PREVIOUS_COMMITTED &&
        iRet != RS_RET_SUSPENDED && iRet != RS_RET_DISABLE_ACTION) {
        LogError(0, iRet, "omclickhouse: could not process message, suspending the action");
        if (pWrkrData->pData->bulkmode) {
            pWrkrData->batch.failedInTx = 1;
            iRet = RS_RET_DEFER_COMMIT;
        } else {
            iRet = RS_RET_SUSPENDED;
        }
    }
ENDdoAction


BEGINendTransaction
    CODESTARTendTransaction;
    if (pWrkrData->batch.failedInTx) {
        initializeBatch(pWrkrData);
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }
    /* End Transaction only if batch data is not empty */
    if (pWrkrData->batch.data != NULL && pWrkrData->batch.nmemb > 0) {
        CHKiRet(submitBatch(pWrkrData, 0));
    } else {
        dbgprintf(
            "omclickhouse: endTransaction, pWrkrData->batch.data is NULL, "
            "nothing to send. \n");
    }
finalize_it:
    pWrkrData->batch.heldInTx = 0;
    pWrkrData->batch.failedInTx = 0;
    /* Only OK and SUSPENDED: the engine's retry loop does not handle
     * RS_RET_DATAFAIL from here.
     */
    if (iRet != RS_RET_OK && iRet != RS_RET_SUSPENDED && iRet != RS_RET_DISABLE_ACTION) {
        LogError(0, iRet, "omclickhouse: could not send batch, suspending the action");
        iRet = RS_RET_SUSPENDED;
    }
ENDendTransaction

static void ATTR_NONNULL() setInstParamDefaults(instanceData *const pData) {
    pData->serverBaseUrl = NULL;
    pData->port = 8123;
    pData->user = NULL;
    pData->pwd = NULL;
    pData->healthCheckTimeout = 3500;
    pData->timeout = 0;
    pData->authBuf = NULL;
    pData->tplName = NULL;
    pData->useHttps = 1;
    pData->allowUnsignedCerts = 0;
    pData->skipVerifyHost = 0;
    pData->errorFile = NULL;
    pData->bulkmode = 1;
    pData->maxbytes = 104857600;  // 100MB
    pData->maxConsecutiveRejects = 0;
    pData->caCertFile = NULL;
    pData->myCertFile = NULL;
    pData->myPrivKeyFile = NULL;
}

/* POST result string ... useful for debugging */
static size_t curlResult(void *ptr, size_t size, size_t nmemb, void *userdata) {
    char *p = (char *)ptr;
    wrkrInstanceData_t *pWrkrData = (wrkrInstanceData_t *)userdata;
    char *buf;
    size_t newlen;
    newlen = pWrkrData->replyLen + size * nmemb;
    if ((buf = realloc(pWrkrData->reply, newlen + 1)) == NULL) {
        LogError(errno, RS_RET_ERR, "omclickhouse: realloc failed in curlResult");
        return 0; /* abort due to failure */
    }
    memcpy(buf + pWrkrData->replyLen, p, size * nmemb);
    pWrkrData->replyLen = newlen;
    pWrkrData->reply = buf;
    return size * nmemb;
}

/* Keep X-ClickHouse-Exception-Code of the insert reply. Header lines are not
 * NUL terminated.
 */
static size_t curlHeaderCb(char *buffer, size_t size, size_t nitems, void *userdata) {
    static const char name[] = "X-ClickHouse-Exception-Code:";
    wrkrInstanceData_t *const pWrkrData = (wrkrInstanceData_t *)userdata;
    const size_t len = size * nitems;
    char value[24];
    size_t i;
    size_t n = 0;

    if (len > sizeof(name) - 1 && strncasecmp(buffer, name, sizeof(name) - 1) == 0) {
        for (i = sizeof(name) - 1; i < len && (buffer[i] == ' ' || buffer[i] == '\t'); ++i) {
        }
        for (; i < len && n < sizeof(value) - 1 && buffer[i] >= '0' && buffer[i] <= '9'; ++i) {
            value[n++] = buffer[i];
        }
        value[n] = '\0';
        if (n > 0) pWrkrData->exceptionCode = strtol(value, NULL, 10);
    }
    return len;
}

static void ATTR_NONNULL() curlSetupCommon(wrkrInstanceData_t *const pWrkrData, CURL *const handle) {
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, pWrkrData->curlHeader);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curlResult);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, pWrkrData);
    if (pWrkrData->pData->allowUnsignedCerts) curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
    if (pWrkrData->pData->skipVerifyHost) curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);
    if (pWrkrData->pData->authBuf != NULL) {
        curl_easy_setopt(handle, CURLOPT_USERPWD, pWrkrData->pData->authBuf);
        curl_easy_setopt(handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    }

    if (pWrkrData->pData->caCertFile) {
        curl_easy_setopt(handle, CURLOPT_CAINFO, pWrkrData->pData->caCertFile);
    }
    if (pWrkrData->pData->myCertFile) {
        curl_easy_setopt(handle, CURLOPT_SSLCERT, pWrkrData->pData->myCertFile);
    }
    if (pWrkrData->pData->myPrivKeyFile) {
        curl_easy_setopt(handle, CURLOPT_SSLKEY, pWrkrData->pData->myPrivKeyFile);
    }
    /* uncomment for in-dept debuggung:
    curl_easy_setopt(handle, CURLOPT_VERBOSE, TRUE); */
}


static void ATTR_NONNULL() curlCheckConnSetup(wrkrInstanceData_t *const pWrkrData) {
    curlSetupCommon(pWrkrData, pWrkrData->curlCheckConnHandle);
    curl_easy_setopt(pWrkrData->curlCheckConnHandle, CURLOPT_TIMEOUT_MS, pWrkrData->pData->healthCheckTimeout);
}


static void ATTR_NONNULL(1) curlPostSetup(wrkrInstanceData_t *const pWrkrData) {
    curlSetupCommon(pWrkrData, pWrkrData->curlPostHandle);
    curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_HEADERFUNCTION, curlHeaderCb);
    curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_HEADERDATA, pWrkrData);
    curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_POST, 1L);
    if (pWrkrData->pData->timeout) {
        curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_TIMEOUT_MS, pWrkrData->pData->timeout);
    }
}

#define CONTENT_JSON "Content-Type: application/json; charset=utf-8"

static rsRetVal ATTR_NONNULL() curlSetup(wrkrInstanceData_t *const pWrkrData) {
    DEFiRet;
    pWrkrData->curlHeader = curl_slist_append(NULL, CONTENT_JSON);
    CHKmalloc(pWrkrData->curlPostHandle = curl_easy_init());
    curlPostSetup(pWrkrData);

    CHKmalloc(pWrkrData->curlCheckConnHandle = curl_easy_init());
    curlCheckConnSetup(pWrkrData);

finalize_it:
    if (iRet != RS_RET_OK && pWrkrData->curlPostHandle != NULL) {
        curl_easy_cleanup(pWrkrData->curlPostHandle);
        pWrkrData->curlPostHandle = NULL;
    }
    RETiRet;
}

static rsRetVal computeAuthHeader(char *user, char *pwd, uchar **authBuf) {
    DEFiRet;
    int r;

    es_str_t *auth = es_newStr(1024);
    if (auth == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "omclickhouse: failed to allocate es_str auth for auth header construction");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    r = es_addBuf(&auth, user, strlen(user));
    if (r == 0) r = es_addChar(&auth, ':');
    if (r == 0 && pwd != NULL) r = es_addBuf(&auth, pwd, strlen(pwd));
    if (r == 0) *authBuf = (uchar *)es_str2cstr(auth, NULL);

    if (r != 0 || *authBuf == NULL) {
        LogError(0, RS_RET_ERR, "omclickhouse: failed to build auth header\n");
        ABORT_FINALIZE(RS_RET_ERR);
    }

finalize_it:
    if (auth != NULL) es_deleteStr(auth);
    RETiRet;
}

/* Build basic URL part, which includes hostname, port, user, pwd as follows:
 * http://user:pwd@hostname:port/ based on a server param
 * Newly creates a cstr for this purpose.
 */
static rsRetVal computeBaseUrl(const char *server, const int port, const sbool useHttps, instanceData *pData) {
#define SCHEME_HTTPS "https://"
#define SCHEME_HTTP "http://"

    char portBuf[64];
    int r = 0;
    const char *host = server;
    DEFiRet;

    assert(server[strlen(server) - 1] != '/');

    es_str_t *urlBuf = es_newStr(256);
    if (urlBuf == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "omclickhouse: failed to allocate es_str urlBuf in computeBaseUrl");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    /* Find where the hostname/ip of the server starts. If the scheme is not specified
      in the uri, start the buffer with a scheme corresponding to the useHttps parameter.
    */
    if (strcasestr(server, SCHEME_HTTP)) {
        host = server + strlen(SCHEME_HTTP);
    } else if (strcasestr(server, SCHEME_HTTPS)) {
        host = server + strlen(SCHEME_HTTPS);
    } else {
        r = useHttps ? es_addBuf(&urlBuf, SCHEME_HTTPS, sizeof(SCHEME_HTTPS) - 1)
                     : es_addBuf(&urlBuf, SCHEME_HTTP, sizeof(SCHEME_HTTP) - 1);
    }
    if (r == 0) r = es_addBuf(&urlBuf, (char *)server, strlen(server));
    if (r == 0 && !strchr(host, ':')) {
        snprintf(portBuf, sizeof(portBuf), ":%d", port);
        r = es_addBuf(&urlBuf, portBuf, strlen(portBuf));
    }
    if (r == 0) r = es_addChar(&urlBuf, '/');
    if (r == 0) pData->serverBaseUrl = (uchar *)es_str2cstr(urlBuf, NULL);

    if (r != 0 || pData->serverBaseUrl == NULL) {
        LogError(0, RS_RET_ERR,
                 "omclickhouse: error occurred computing baseUrl from "
                 "server %s",
                 server);
        ABORT_FINALIZE(RS_RET_ERR);
    }
finalize_it:
    if (urlBuf) {
        es_deleteStr(urlBuf);
    }
    RETiRet;
}

BEGINnewActInst
    struct cnfparamvals *pvals;
    uchar *server = NULL;
    int i;
    FILE *fp;
    char errStr[1024];
    CODESTARTnewActInst;
    if ((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    CHKiRet(createInstance(&pData));
    setInstParamDefaults(pData);

    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(actpblk.descr[i].name, "server")) {
            CHKmalloc(server = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(actpblk.descr[i].name, "port")) {
            pData->port = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "user")) {
            CHKmalloc(pData->user = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(actpblk.descr[i].name, "pwd")) {
            CHKmalloc(pData->pwd = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(actpblk.descr[i].name, "healthchecktimeout")) {
            pData->healthCheckTimeout = (long)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "timeout")) {
            pData->timeout = (long)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "template")) {
            CHKmalloc(pData->tplName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(actpblk.descr[i].name, "usehttps")) {
            pData->useHttps = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "allowunsignedcerts")) {
            pData->allowUnsignedCerts = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "skipverifyhost")) {
            pData->skipVerifyHost = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "errorfile")) {
            CHKmalloc(pData->errorFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(actpblk.descr[i].name, "bulkmode")) {
            pData->bulkmode = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "maxbytes")) {
            pData->maxbytes = (size_t)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "maxconsecutiverejects")) {
            pData->maxConsecutiveRejects = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "tls.cacert")) {
            CHKmalloc(pData->caCertFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
            fp = fopen((const char *)pData->caCertFile, "r");
            if (fp == NULL) {
                rs_strerror_r(errno, errStr, sizeof(errStr));
                LogError(0, RS_RET_NO_FILE_ACCESS,
                         "error: omclickhouse: 'tls.cacert' file %s couldn't be accessed: %s\n", pData->caCertFile,
                         errStr);
            } else {
                fclose(fp);
            }
        } else if (!strcmp(actpblk.descr[i].name, "tls.mycert")) {
            CHKmalloc(pData->myCertFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
            fp = fopen((const char *)pData->myCertFile, "r");
            if (fp == NULL) {
                rs_strerror_r(errno, errStr, sizeof(errStr));
                LogError(0, RS_RET_NO_FILE_ACCESS,
                         "error: omclickhouse: 'tls.mycert' file %s couldn't be accessed: %s\n", pData->myCertFile,
                         errStr);
            } else {
                fclose(fp);
            }
        } else if (!strcmp(actpblk.descr[i].name, "tls.myprivkey")) {
            CHKmalloc(pData->myPrivKeyFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
            fp = fopen((const char *)pData->myPrivKeyFile, "r");
            if (fp == NULL) {
                rs_strerror_r(errno, errStr, sizeof(errStr));
                LogError(0, RS_RET_NO_FILE_ACCESS,
                         "error: omclickhouse: 'tls.myprivkey' file %s couldn't be accessed: %s\n",
                         pData->myPrivKeyFile, errStr);
            } else {
                fclose(fp);
            }
        } else {
            LogError(0, RS_RET_INTERNAL_ERROR,
                     "omclickhouse: program error, "
                     "non-handled param '%s'",
                     actpblk.descr[i].name);
        }
    }


    if (pData->user == NULL && pData->pwd != NULL) {
        LogMsg(0, RS_RET_OK, LOG_WARNING,
               "omclickhouse: No user was specified "
               "but a password was given.");
    }

    if (pData->user != NULL) CHKiRet(computeAuthHeader((char *)pData->user, (char *)pData->pwd, &pData->authBuf));

    CODE_STD_STRING_REQUESTnewActInst(1);
    CHKiRet(OMSRsetEntry(*ppOMSR, 0,
                         (uchar *)strdup((pData->tplName == NULL) ? " StdClickHouseFmt" : (char *)pData->tplName),
                         OMSR_RQD_TPL_OPT_SQL_BACKSLASH));

    if (server != NULL) {
        CHKiRet(computeBaseUrl((const char *)server, pData->port, pData->useHttps, pData));
    } else {
        LogMsg(0, RS_RET_OK, LOG_WARNING, "omclickhouse: No servers specified, using localhost");
        CHKiRet(computeBaseUrl("localhost", pData->port, pData->useHttps, pData));
    }

    /* node created, let's add to list of instance configs for the module */
    if (loadModConf->tail == NULL) {
        loadModConf->tail = loadModConf->root = pData;
    } else {
        loadModConf->tail->next = pData;
        loadModConf->tail = pData;
    }

    CODE_STD_FINALIZERnewActInst;
    free(server);
    cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
    pModConf->root = pModConf->tail = NULL;
ENDbeginCnfLoad


BEGINendCnfLoad
    CODESTARTendCnfLoad;
    loadModConf = NULL; /* done loading */
ENDendCnfLoad


BEGINcheckCnf
    CODESTARTcheckCnf;
ENDcheckCnf


BEGINactivateCnf
    CODESTARTactivateCnf;
ENDactivateCnf


BEGINfreeCnf
    CODESTARTfreeCnf;
ENDfreeCnf


BEGINdoHUP
    CODESTARTdoHUP;
    if (pData->fdErrFile != -1) {
        close(pData->fdErrFile);
        pData->fdErrFile = -1;
    }
ENDdoHUP


BEGINmodExit
    CODESTARTmodExit;
    curl_global_cleanup();
    statsobj.Destruct(&indexStats);
    objRelease(statsobj, CORE_COMPONENT);
    objRelease(prop, CORE_COMPONENT);
    objRelease(ruleset, CORE_COMPONENT);
ENDmodExit

NO_LEGACY_CONF_parseSelectorAct

    BEGINqueryEtryPt CODESTARTqueryEtryPt;
CODEqueryEtryPt_STD_OMOD_QUERIES;
CODEqueryEtryPt_STD_OMOD8_QUERIES;
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES;
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
CODEqueryEtryPt_doHUP CODEqueryEtryPt_TXIF_OMOD_QUERIES /* we support the transactional interface! */
    CODEqueryEtryPt_STD_CONF2_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr CHKiRet(objUse(statsobj, CORE_COMPONENT));
    CHKiRet(objUse(prop, CORE_COMPONENT));
    CHKiRet(objUse(ruleset, CORE_COMPONENT));

    if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
        LogError(0, RS_RET_OBJ_CREATION_FAILED, "CURL fail. -indexing disabled");
        ABORT_FINALIZE(RS_RET_OBJ_CREATION_FAILED);
    }

    /* support statistics gathering */
    CHKiRet(statsobj.Construct(&indexStats));
    CHKiRet(statsobj.SetName(indexStats, (uchar *)"omclickhouse"));
    CHKiRet(statsobj.SetOrigin(indexStats, (uchar *)"omclickhouse"));
    STATSCOUNTER_INIT(indexSubmit, mutIndexSubmit);
    CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"submitted", ctrType_IntCtr, CTR_FLAG_RESETTABLE, &indexSubmit));
    STATSCOUNTER_INIT(indexHTTPFail, mutIndexHTTPFail);
    CHKiRet(
        statsobj.AddCounter(indexStats, (uchar *)"failed.http", ctrType_IntCtr, CTR_FLAG_RESETTABLE, &indexHTTPFail));
    STATSCOUNTER_INIT(indexHTTPReqFail, mutIndexHTTPReqFail);
    CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"failed.httprequests", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &indexHTTPReqFail));
    STATSCOUNTER_INIT(indexFail, mutIndexFail);
    CHKiRet(
        statsobj.AddCounter(indexStats, (uchar *)"failed.clickhouse", ctrType_IntCtr, CTR_FLAG_RESETTABLE, &indexFail));
    STATSCOUNTER_INIT(indexSuccess, mutIndexSuccess);
    CHKiRet(statsobj.AddCounter(indexStats, (uchar *)"response.success", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &indexSuccess));
    CHKiRet(statsobj.ConstructFinalize(indexStats));

ENDmodInit

/* vi:set ai:
 */
