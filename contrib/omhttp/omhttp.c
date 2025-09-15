/* omhttp.c
 * This is an http output module based on omelasticsearch
 *
 * NOTE: read comments in module-template.h for more specifics!
 *
 * Supports profile-based configuration for common HTTP endpoints:
 * - profile="loki" for Grafana Loki
 * - profile="hec:splunk" for Splunk HTTP Event Collector (proof-of-concept only, see
 * https://github.com/rsyslog/rsyslog/issues/5756 for feedback)
 *
 * Copyright 2011 Nathan Scott.
 * Copyright 2009-2018 Rainer Gerhards and Adiscon GmbH.
 * Copyright 2018 Christian Tramnitz
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
#include <zlib.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "unicode-helper.h"
#include "obj-types.h"
#include "ratelimit.h"
#include "ruleset.h"
#include "statsobj.h"

#ifndef O_LARGEFILE
    #define O_LARGEFILE 0
#endif

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("omhttp")

/* internal structures */
DEF_OMOD_STATIC_DATA;
DEFobjCurrIf(prop) DEFobjCurrIf(ruleset) DEFobjCurrIf(statsobj)


    typedef struct _targetStats {
    statsobj_t *defaultstats;

    STATSCOUNTER_DEF(ctrMessagesSubmitted, mutCtrMessagesSubmitted);  // Number of message submitted to module
    STATSCOUNTER_DEF(ctrMessagesSuccess, mutCtrMessagesSuccess);  // Number of messages successfully sent
    STATSCOUNTER_DEF(ctrMessagesFail, mutCtrMessagesFail);  // Number of messages that failed to send
    STATSCOUNTER_DEF(ctrMessagesRetry, mutCtrMessagesRetry);  // Number of messages requeued for retry
    STATSCOUNTER_DEF(ctrHttpRequestCount, mutCtrHttpRequestCount);  // Number of attempted HTTP requests
    STATSCOUNTER_DEF(ctrHttpRequestSuccess, mutCtrHttpRequestSuccess);  // Number of successful HTTP requests
    STATSCOUNTER_DEF(ctrHttpRequestFail, mutCtrHttpRequestFail);  // Number of failed HTTP req, 4XX+ are NOT failures
    STATSCOUNTER_DEF(ctrHttpStatusSuccess, mutCtrHttpStatusSuccess);  // Number of requests returning 1XX/2XX status
    STATSCOUNTER_DEF(ctrHttpStatusFail, mutCtrHttpStatusFail);  // Number of requests returning 300+ status
    STATSCOUNTER_DEF(ctrHttpRequestsCount, mutCtrHttpRequestsCount);  // Number of attempted HTTP requests
    STATSCOUNTER_DEF(httpRequestsBytes, mutHttpRequestsBytes);  // Number of bytes in HTTP requests
    STATSCOUNTER_DEF(httpRequestsTimeMs, mutHttpRequestsTimeMs);  // Number of Times(ms) in HTTP requests
    STATSCOUNTER_DEF(ctrHttpRequestsStatus0xx, mutCtrHttpRequestsStatus0xx);  // HTTP requests returning 0xx
    STATSCOUNTER_DEF(ctrHttpRequestsStatus1xx, mutCtrHttpRequestsStatus1xx);  // HTTP requests returning 1xx
    STATSCOUNTER_DEF(ctrHttpRequestsStatus2xx, mutCtrHttpRequestsStatus2xx);  // HTTP requests returning 2xx
    STATSCOUNTER_DEF(ctrHttpRequestsStatus3xx, mutCtrHttpRequestsStatus3xx);  // HTTP requests returning 3xx
    STATSCOUNTER_DEF(ctrHttpRequestsStatus4xx, mutCtrHttpRequestsStatus4xx);  // HTTP requests returning 4xx
    STATSCOUNTER_DEF(ctrHttpRequestsStatus5xx, mutCtrHttpRequestsStatus5xx);  // HTTP requests returning 5xx

} targetStats_t;


static prop_t *pInputName = NULL;
static int omhttpInstancesCnt = 0;

#define WRKR_DATA_TYPE_ES 0xBADF0001

#define HTTP_HEADER_CONTENT_JSON "Content-Type: application/json; charset=utf-8"
#define HTTP_HEADER_CONTENT_TEXT "Content-Type: text/plain"
#define HTTP_HEADER_CONTENT_KAFKA "Content-Type: application/vnd.kafka.v1+json"
#define HTTP_HEADER_ENCODING_GZIP "Content-Encoding: gzip"
#define HTTP_HEADER_EXPECT_EMPTY "Expect:"

#define VALID_BATCH_FORMATS "newline jsonarray kafkarest lokirest"

/* Default batch size constants */
#define DEFAULT_MAX_BATCH_BYTES (10 * 1024 * 1024) /* 10 MB - default max message size for AWS API Gateway */
#define SPLUNK_HEC_MAX_BATCH_BYTES (1024 * 1024) /* 1 MB - Splunk HEC recommended limit */
typedef enum batchFormat_e { FMT_NEWLINE, FMT_JSONARRAY, FMT_KAFKAREST, FMT_LOKIREST } batchFormat_t;

/* REST API uses this URL:
 * https://<hostName>:<restPort>/restPath
 */
typedef struct curl_slist HEADER;
typedef struct instanceConf_s {
    int defaultPort;
    int fdErrFile; /* error file fd or -1 if not open */
    pthread_mutex_t mutErrFile;
    uchar **serverBaseUrls;
    int numServers;
    long healthCheckTimeout;
    long restPathTimeout;
    uchar *uid;
    uchar *pwd;
    uchar *authBuf;
    uchar *httpcontenttype;
    uchar *headerContentTypeBuf;
    uchar *httpheaderkey;
    uchar *httpheadervalue;
    uchar *headerBuf;
    uchar **httpHeaders;
    int nHttpHeaders;
    uchar *restPath;
    uchar *checkPath;
    uchar *proxyHost;
    int proxyPort;
    uchar *tplName;
    uchar *errorFile;
    sbool batchMode;
    uchar *batchFormatName;
    batchFormat_t batchFormat;
    sbool bFreeBatchFormatName;
    sbool dynRestPath;
    size_t maxBatchBytes;
    size_t maxBatchSize;
    sbool compress;
    int compressionLevel; /* Compression level for zlib, default=-1, fastest=1, best=9, none=0*/
    sbool useHttps;
    sbool allowUnsignedCerts;
    sbool skipVerifyHost;
    uchar *caCertFile;
    uchar *myCertFile;
    uchar *myPrivKeyFile;
    sbool reloadOnHup;
    sbool retryFailures;
    sbool retryAddMetadata;
    int nhttpRetryCodes;
    unsigned int *httpRetryCodes;
    int nIgnorableCodes;
    unsigned int *ignorableCodes;
    unsigned int ratelimitInterval;
    unsigned int ratelimitBurst;
    /* for retries */
    ratelimit_t *ratelimiter;
    uchar *retryRulesetName;
    ruleset_t *retryRuleset;
    struct instanceConf_s *next;

    uchar *statsName;
    /* Stats Counter */
    targetStats_t *listObjStats;
    sbool statsBySenders;
} instanceData;

struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
    instanceConf_t *root, *tail;
};
static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */

typedef struct wrkrInstanceData {
    PTR_ASSERT_DEF
    instanceData *pData;
    int serverIndex;
    int replyLen;
    char *reply;
    long httpStatusCode; /* http status code of response */
    CURL *curlCheckConnHandle; /* libcurl session handle for checking the server connection */
    CURL *curlPostHandle; /* libcurl session handle for posting data to the server */
    HEADER *curlHeader; /* json POST request info */
    uchar *restURL; /* last used URL for error reporting */
    sbool bzInitDone;
    z_stream zstrm; /* zip stream to use for gzip http compression */
    struct {
        uchar **data; /* array of strings, this will be batched up lazily */
        uchar *restPath; /* Helper for restpath in batch mode */
        size_t sizeBytes; /* total length of this batch in bytes */
        size_t nmemb; /* number of messages in batch (for statistics counting) */

    } batch;
    struct {
        uchar *buf;
        size_t curLen;
        size_t len;
    } compressCtx;
} wrkrInstanceData_t;

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
    {"server", eCmdHdlrArray, 0},
    {"serverport", eCmdHdlrInt, 0},
    {"healthchecktimeout", eCmdHdlrInt, 0},
    {"restpathtimeout", eCmdHdlrInt, 0},
    {"httpcontenttype", eCmdHdlrGetWord, 0},
    {"httpheaderkey", eCmdHdlrGetWord, 0},
    {"httpheadervalue", eCmdHdlrString, 0},
    {"httpheaders", eCmdHdlrArray, 0},
    {"uid", eCmdHdlrGetWord, 0},
    {"pwd", eCmdHdlrGetWord, 0},
    {"restpath", eCmdHdlrGetWord, 0},
    {"checkpath", eCmdHdlrGetWord, 0},
    {"dynrestpath", eCmdHdlrBinary, 0},
    {"proxyhost", eCmdHdlrString, 0},
    {"proxyport", eCmdHdlrInt, 0},
    {"batch", eCmdHdlrBinary, 0},
    {"batch.format", eCmdHdlrGetWord, 0},
    {"batch.maxbytes", eCmdHdlrSize, 0},
    {"batch.maxsize", eCmdHdlrSize, 0},
    {"compress", eCmdHdlrBinary, 0},
    {"compress.level", eCmdHdlrInt, 0},
    {"usehttps", eCmdHdlrBinary, 0},
    {"errorfile", eCmdHdlrGetWord, 0},
    {"template", eCmdHdlrGetWord, 0},
    {"allowunsignedcerts", eCmdHdlrBinary, 0},
    {"skipverifyhost", eCmdHdlrBinary, 0},
    {"tls.cacert", eCmdHdlrString, 0},
    {"tls.mycert", eCmdHdlrString, 0},
    {"tls.myprivkey", eCmdHdlrString, 0},
    {"reloadonhup", eCmdHdlrBinary, 0},
    {"httpretrycodes", eCmdHdlrArray, 0},
    {"retry", eCmdHdlrBinary, 0},
    {"retry.addmetadata", eCmdHdlrBinary, 0},
    {"retry.ruleset", eCmdHdlrString, 0},
    {"ratelimit.interval", eCmdHdlrInt, 0},
    {"ratelimit.burst", eCmdHdlrInt, 0},
    {"name", eCmdHdlrGetWord, 0},
    {"httpignorablecodes", eCmdHdlrArray, 0},
    {"profile", eCmdHdlrGetWord, 0},
    {"statsbysenders", eCmdHdlrBinary, 0},
};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

static rsRetVal curlSetup(wrkrInstanceData_t *pWrkrData);
static void curlCleanup(wrkrInstanceData_t *pWrkrData);
static void curlCheckConnSetup(wrkrInstanceData_t *const pWrkrData);

/* compressCtx functions */
static void ATTR_NONNULL() initCompressCtx(wrkrInstanceData_t *pWrkrData);

static void ATTR_NONNULL() freeCompressCtx(wrkrInstanceData_t *pWrkrData);

static rsRetVal ATTR_NONNULL() resetCompressCtx(wrkrInstanceData_t *pWrkrData, size_t len);

static rsRetVal ATTR_NONNULL() growCompressCtx(wrkrInstanceData_t *pWrkrData, size_t newLen);

static rsRetVal ATTR_NONNULL() appendCompressCtx(wrkrInstanceData_t *pWrkrData, uchar *srcBuf, size_t srcLen);

BEGINcreateInstance
    CODESTARTcreateInstance;
    pData->fdErrFile = -1;
    pthread_mutex_init(&pData->mutErrFile, NULL);
    pData->caCertFile = NULL;
    pData->myCertFile = NULL;
    pData->myPrivKeyFile = NULL;
    pData->ratelimiter = NULL;
    pData->retryRulesetName = NULL;
    pData->retryRuleset = NULL;
ENDcreateInstance

BEGINcreateWrkrInstance
    uchar **batchData;
    CODESTARTcreateWrkrInstance;
    PTR_ASSERT_SET_TYPE(pWrkrData, WRKR_DATA_TYPE_ES);
    pWrkrData->curlHeader = NULL;
    pWrkrData->curlPostHandle = NULL;
    pWrkrData->curlCheckConnHandle = NULL;
    pWrkrData->serverIndex = 0;
    pWrkrData->httpStatusCode = 0;
    pWrkrData->restURL = NULL;
    pWrkrData->bzInitDone = 0;
    if (pData->batchMode) {
        pWrkrData->batch.nmemb = 0;
        pWrkrData->batch.sizeBytes = 0;
        batchData = (uchar **)malloc(pData->maxBatchSize * sizeof(uchar *));
        if (batchData == NULL) {
            LogError(0, RS_RET_OUT_OF_MEMORY,
                     "omhttp: cannot allocate memory for batch queue turning off batch mode\n");
            pData->batchMode = 0; /* at least it works */
        } else {
            pWrkrData->batch.data = batchData;
            pWrkrData->batch.restPath = NULL;
        }
    }
    initCompressCtx(pWrkrData);
    iRet = curlSetup(pWrkrData);
ENDcreateWrkrInstance

BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURERepeatedMsgReduction) iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINfreeInstance
    int i;
    CODESTARTfreeInstance;
    if (pData->fdErrFile != -1) close(pData->fdErrFile);
    pthread_mutex_destroy(&pData->mutErrFile);
    for (i = 0; i < pData->numServers; ++i) free(pData->serverBaseUrls[i]);
    free(pData->serverBaseUrls);
    free(pData->uid);
    free(pData->httpcontenttype);
    free(pData->headerContentTypeBuf);
    free(pData->httpheaderkey);
    free(pData->httpheadervalue);
    for (i = 0; i < pData->nHttpHeaders; ++i) {
        free((void *)pData->httpHeaders[i]);
    }
    free(pData->httpHeaders);
    pData->nHttpHeaders = 0;
    free(pData->pwd);
    free(pData->authBuf);
    free(pData->headerBuf);
    free(pData->restPath);
    free(pData->checkPath);
    free(pData->proxyHost);
    free(pData->tplName);
    free(pData->errorFile);
    free(pData->caCertFile);
    free(pData->myCertFile);
    free(pData->myPrivKeyFile);
    free(pData->httpRetryCodes);
    free(pData->retryRulesetName);
    free(pData->ignorableCodes);
    if (pData->ratelimiter != NULL) ratelimitDestruct(pData->ratelimiter);
    if (pData->bFreeBatchFormatName) free(pData->batchFormatName);
    if (pData->listObjStats != NULL) {
        for (int j = 0; j < pData->numServers; ++j) {
            if (pData->listObjStats[j].defaultstats != NULL) statsobj.Destruct(&(pData->listObjStats[j].defaultstats));
        }
        free(pData->listObjStats);
    }
    free(pData->statsName);
ENDfreeInstance

BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
    curlCleanup(pWrkrData);

    free(pWrkrData->restURL);
    pWrkrData->restURL = NULL;

    free(pWrkrData->batch.data);
    pWrkrData->batch.data = NULL;

    if (pWrkrData->batch.restPath != NULL) {
        free(pWrkrData->batch.restPath);
        pWrkrData->batch.restPath = NULL;
    }

    if (pWrkrData->bzInitDone) deflateEnd(&pWrkrData->zstrm);
    freeCompressCtx(pWrkrData);

ENDfreeWrkrInstance

BEGINdbgPrintInstInfo
    int i;
    CODESTARTdbgPrintInstInfo;
    dbgprintf("omhttp\n");
    dbgprintf("\ttemplate='%s'\n", pData->tplName);
    dbgprintf("\tnumServers=%d\n", pData->numServers);
    dbgprintf("\thealthCheckTimeout=%lu\n", pData->healthCheckTimeout);
    dbgprintf("\trestPathTimeout=%lu\n", pData->restPathTimeout);
    dbgprintf("\tserverBaseUrls=");
    for (i = 0; i < pData->numServers; ++i) dbgprintf("%c'%s'", i == 0 ? '[' : ' ', pData->serverBaseUrls[i]);
    dbgprintf("]\n");
    dbgprintf("\tdefaultPort=%d\n", pData->defaultPort);
    dbgprintf("\tuid='%s'\n", pData->uid == NULL ? (uchar *)"(not configured)" : pData->uid);
    dbgprintf("\thttpcontenttype='%s'\n",
              pData->httpcontenttype == NULL ? (uchar *)"(not configured)" : pData->httpcontenttype);
    dbgprintf("\thttpheaderkey='%s'\n",
              pData->httpheaderkey == NULL ? (uchar *)"(not configured)" : pData->httpheaderkey);
    dbgprintf("\thttpheadervalue='%s'\n",
              pData->httpheadervalue == NULL ? (uchar *)"(not configured)" : pData->httpheadervalue);
    dbgprintf("\thttpHeaders=[");
    for (i = 0; i < pData->nHttpHeaders; ++i) dbgprintf("\t%s\n", pData->httpHeaders[i]);
    dbgprintf("\t]\n");
    dbgprintf("\tpwd=(%sconfigured)\n", pData->pwd == NULL ? "not " : "");
    dbgprintf("\trest path='%s'\n", pData->restPath);
    dbgprintf("\tcheck path='%s'\n", pData->checkPath);
    dbgprintf("\tdynamic rest path=%d\n", pData->dynRestPath);
    dbgprintf("\tproxy host='%s'\n", (pData->proxyHost == NULL) ? "unset" : (char *)pData->proxyHost);
    dbgprintf("\tproxy port='%d'\n", pData->proxyPort);
    dbgprintf("\tuse https=%d\n", pData->useHttps);
    dbgprintf("\tbatch=%d\n", pData->batchMode);
    dbgprintf("\tbatch.format='%s'\n", pData->batchFormatName);
    dbgprintf("\tbatch.maxbytes=%zu\n", pData->maxBatchBytes);
    dbgprintf("\tbatch.maxsize=%zu\n", pData->maxBatchSize);
    dbgprintf("\tcompress=%d\n", pData->compress);
    dbgprintf("\tcompress.level=%d\n", pData->compressionLevel);
    dbgprintf("\tallowUnsignedCerts=%d\n", pData->allowUnsignedCerts);
    dbgprintf("\tskipVerifyHost=%d\n", pData->skipVerifyHost);
    dbgprintf("\terrorfile='%s'\n", pData->errorFile == NULL ? (uchar *)"(not configured)" : pData->errorFile);
    dbgprintf("\ttls.cacert='%s'\n", pData->caCertFile);
    dbgprintf("\ttls.mycert='%s'\n", pData->myCertFile);
    dbgprintf("\ttls.myprivkey='%s'\n", pData->myPrivKeyFile);
    dbgprintf("\treloadonhup='%d'\n", pData->reloadOnHup);
    for (i = 0; i < pData->nhttpRetryCodes; ++i) dbgprintf("%c'%d'", i == 0 ? '[' : ' ', pData->httpRetryCodes[i]);
    dbgprintf("]\n");
    dbgprintf("\tretry='%d'\n", pData->retryFailures);
    dbgprintf("\tretry.addmetadata='%d'\n", pData->retryAddMetadata);
    dbgprintf("\tretry.ruleset='%s'\n", pData->retryRulesetName);
    dbgprintf("\tratelimit.interval='%u'\n", pData->ratelimitInterval);
    dbgprintf("\tratelimit.burst='%u'\n", pData->ratelimitBurst);
    for (i = 0; i < pData->nIgnorableCodes; ++i) dbgprintf("%c'%d'", i == 0 ? '[' : ' ', pData->ignorableCodes[i]);
    dbgprintf("]\n");
    dbgprintf("\tratelimit.interval='%d'\n", pData->ratelimitInterval);
    dbgprintf("\tratelimit.burst='%d'\n", pData->ratelimitBurst);
    dbgprintf("\tstatsname='%s'\n", pData->statsName);
    dbgprintf("\tstatsbysenders='%d'\n", pData->statsBySenders);
ENDdbgPrintInstInfo


/* http POST result string ... useful for debugging */
static size_t curlResult(void *ptr, size_t size, size_t nmemb, void *userdata) {
    char *p = (char *)ptr;
    wrkrInstanceData_t *pWrkrData = (wrkrInstanceData_t *)userdata;
    char *buf;
    size_t newlen;
    PTR_ASSERT_CHK(pWrkrData, WRKR_DATA_TYPE_ES);
    newlen = pWrkrData->replyLen + size * nmemb;
    if ((buf = realloc(pWrkrData->reply, newlen + 1)) == NULL) {
        LogError(errno, RS_RET_ERR, "omhttp: realloc failed in curlResult");
        return 0; /* abort due to failure */
    }
    memcpy(buf + pWrkrData->replyLen, p, size * nmemb);
    pWrkrData->replyLen = newlen;
    pWrkrData->reply = buf;
    return size * nmemb;
}

/* Build basic URL part, which includes hostname and port as follows:
 * http://hostname:port/ based on a server param
 * Newly creates a cstr for this purpose.
 * Note: serverParam MUST NOT end in '/' (caller must strip if it exists)
 */
static rsRetVal computeBaseUrl(const char *const serverParam,
                               const int defaultPort,
                               const sbool useHttps,
                               uchar **baseUrl) {
#define SCHEME_HTTPS "https://"
#define SCHEME_HTTP "http://"

    char portBuf[64];
    int r = 0;
    const char *host = serverParam;
    DEFiRet;

    assert(serverParam[strlen(serverParam) - 1] != '/');

    es_str_t *urlBuf = es_newStr(256);
    if (urlBuf == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "omhttp: failed to allocate es_str urlBuf in computeBaseUrl");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    /* Find where the hostname/ip of the server starts. If the scheme is not specified
     * in the uri, start the buffer with a scheme corresponding to the useHttps parameter.
     */
    if (strcasestr(serverParam, SCHEME_HTTP))
        host = serverParam + strlen(SCHEME_HTTP);
    else if (strcasestr(serverParam, SCHEME_HTTPS))
        host = serverParam + strlen(SCHEME_HTTPS);
    else
        r = useHttps ? es_addBuf(&urlBuf, SCHEME_HTTPS, sizeof(SCHEME_HTTPS) - 1)
                     : es_addBuf(&urlBuf, SCHEME_HTTP, sizeof(SCHEME_HTTP) - 1);

    if (r == 0) r = es_addBuf(&urlBuf, (char *)serverParam, strlen(serverParam));
    if (r == 0 && !strchr(host, ':')) {
        snprintf(portBuf, sizeof(portBuf), ":%d", defaultPort);
        r = es_addBuf(&urlBuf, portBuf, strlen(portBuf));
    }
    if (r == 0) r = es_addChar(&urlBuf, '/');
    if (r == 0) *baseUrl = (uchar *)es_str2cstr(urlBuf, NULL);

    if (r != 0 || baseUrl == NULL) {
        LogError(0, RS_RET_ERR, "omhttp: error occurred computing baseUrl from server %s", serverParam);
        ABORT_FINALIZE(RS_RET_ERR);
    }
finalize_it:
    if (urlBuf) {
        es_deleteStr(urlBuf);
    }
    RETiRet;
}

static inline void incrementServerIndex(wrkrInstanceData_t *pWrkrData) {
    pWrkrData->serverIndex = (pWrkrData->serverIndex + 1) % pWrkrData->pData->numServers;
}


/* checks if connection to ES can be established; also iterates over
 * potential servers to support high availability (HA) feature. If it
 * needs to switch server, will record new one in curl handle.
 */
static rsRetVal ATTR_NONNULL() checkConn(wrkrInstanceData_t *const pWrkrData) {
    CURL *curl;
    CURLcode res;
    es_str_t *urlBuf = NULL;
    char *healthUrl;
    char *serverUrl;
    char *checkPath;
    int i;
    int r;
    DEFiRet;

    if (pWrkrData->pData->checkPath == NULL) {
        DBGPRINTF("omhttp: checkConn no health check uri configured skipping it\n");
        FINALIZE;
    }

    pWrkrData->reply = NULL;
    pWrkrData->replyLen = 0;
    curl = pWrkrData->curlCheckConnHandle;
    urlBuf = es_newStr(256);
    if (urlBuf == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "omhttp: unable to allocate buffer for health check uri.");
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }

    for (i = 0; i < pWrkrData->pData->numServers; ++i) {
        serverUrl = (char *)pWrkrData->pData->serverBaseUrls[pWrkrData->serverIndex];
        checkPath = (char *)pWrkrData->pData->checkPath;

        es_emptyStr(urlBuf);
        r = es_addBuf(&urlBuf, serverUrl, strlen(serverUrl));
        if (r == 0 && checkPath != NULL) r = es_addBuf(&urlBuf, checkPath, strlen(checkPath));
        if (r == 0) healthUrl = es_str2cstr(urlBuf, NULL);
        if (r != 0 || healthUrl == NULL) {
            LogError(0, RS_RET_OUT_OF_MEMORY, "omhttp: unable to allocate buffer for health check uri.");
            ABORT_FINALIZE(RS_RET_SUSPENDED);
        }

        curlCheckConnSetup(pWrkrData);
        curl_easy_setopt(curl, CURLOPT_URL, healthUrl);
        res = curl_easy_perform(curl);
        free(healthUrl);

        if (res == CURLE_OK) {
            DBGPRINTF(
                "omhttp: checkConn %s completed with success "
                "on attempt %d\n",
                serverUrl, i);
            ABORT_FINALIZE(RS_RET_OK);
        }

        DBGPRINTF("omhttp: checkConn %s failed on attempt %d: %s\n", serverUrl, i, curl_easy_strerror(res));
        incrementServerIndex(pWrkrData);
    }

    LogMsg(0, RS_RET_SUSPENDED, LOG_WARNING, "omhttp: checkConn failed after %d attempts.", i);
    ABORT_FINALIZE(RS_RET_SUSPENDED);

finalize_it:
    if (urlBuf != NULL) es_deleteStr(urlBuf);

    free(pWrkrData->reply);
    pWrkrData->reply = NULL; /* don't leave dangling pointer */
    RETiRet;
}


BEGINtryResume
    CODESTARTtryResume;
    DBGPRINTF("omhttp: tryResume called\n");
    iRet = checkConn(pWrkrData);
ENDtryResume


/* get the current rest path for this message */
static void ATTR_NONNULL(1) getRestPath(const instanceData *const pData, uchar **const tpls, uchar **const restPath) {
    *restPath = pData->restPath;
    if (pData->dynRestPath && tpls != NULL) {
        *restPath = tpls[1];
    }

    assert(restPath != NULL);
    return;
}


static rsRetVal ATTR_NONNULL(1) setPostURL(wrkrInstanceData_t *const pWrkrData, uchar **const tpls) {
    uchar *restPath;
    char *baseUrl;
    es_str_t *url;
    int r;
    DEFiRet;
    instanceData *const pData = pWrkrData->pData;

    baseUrl = (char *)pData->serverBaseUrls[pWrkrData->serverIndex];
    url = es_newStrFromCStr(baseUrl, strlen(baseUrl));
    if (url == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "omhttp: error allocating new estr for POST url.");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    if (pWrkrData->batch.restPath != NULL) {
        /* get from batch if set! */
        restPath = pWrkrData->batch.restPath;
    } else {
        getRestPath(pData, tpls, &restPath);
    }

    r = 0;
    if (restPath != NULL) r = es_addBuf(&url, (char *)restPath, ustrlen(restPath));

    if (r != 0) {
        LogError(0, RS_RET_ERR,
                 "omhttp: failure in creating restURL, "
                 "error code: %d",
                 r);
        ABORT_FINALIZE(RS_RET_ERR);
    }

    if (pWrkrData->restURL != NULL) free(pWrkrData->restURL);

    pWrkrData->restURL = (uchar *)es_str2cstr(url, NULL);
    curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_URL, pWrkrData->restURL);
    DBGPRINTF("omhttp: using REST URL: '%s'\n", pWrkrData->restURL);

finalize_it:
    if (url != NULL) es_deleteStr(url);
    RETiRet;
}

/*
 * Dumps entire bulk request and response in error log
 * {
 *	"request": {
 *	 	"url": "https://url.com:443/path",
 *	 	"postdata": "mypayload" }
 *	 "response" : {
 *	 	"status": 400,
 *	 	"response": "error string" }
 * }
 */
static rsRetVal renderJsonErrorMessage(wrkrInstanceData_t *pWrkrData, uchar *reqmsg, char **rendered) {
    DEFiRet;
    fjson_object *req = NULL;
    fjson_object *res = NULL;
    fjson_object *errRoot = NULL;

    if ((req = fjson_object_new_object()) == NULL) ABORT_FINALIZE(RS_RET_ERR);
    fjson_object_object_add(req, "url", fjson_object_new_string((char *)pWrkrData->restURL));
    fjson_object_object_add(req, "postdata", fjson_object_new_string((char *)reqmsg));

    if ((res = fjson_object_new_object()) == NULL) {
        fjson_object_put(req);  // cleanup request object
        ABORT_FINALIZE(RS_RET_ERR);
    }

#define ERR_MSG_NULL "NULL: curl request failed or no response"
    fjson_object_object_add(res, "status", fjson_object_new_int(pWrkrData->httpStatusCode));
    if (pWrkrData->reply == NULL) {
        fjson_object_object_add(res, "message", fjson_object_new_string_len(ERR_MSG_NULL, strlen(ERR_MSG_NULL)));
    } else {
        fjson_object_object_add(res, "message", fjson_object_new_string_len(pWrkrData->reply, pWrkrData->replyLen));
    }

    if ((errRoot = fjson_object_new_object()) == NULL) {
        fjson_object_put(req);  // cleanup request object
        fjson_object_put(res);  // cleanup response object
        ABORT_FINALIZE(RS_RET_ERR);
    }

    fjson_object_object_add(errRoot, "request", req);
    fjson_object_object_add(errRoot, "response", res);

    CHKmalloc(*rendered = strdup((char *)fjson_object_to_json_string(errRoot)));

finalize_it:
    if (errRoot != NULL) fjson_object_put(errRoot);

    RETiRet;
}

/* write data error request/replies to separate error file
 * Note: we open the file but never close it before exit. If it
 * needs to be closed, HUP must be sent.
 */
static rsRetVal ATTR_NONNULL()
    writeDataError(wrkrInstanceData_t *const pWrkrData, instanceData *const pData, uchar *const reqmsg) {
    char *rendered = NULL;
    size_t toWrite;
    ssize_t wrRet;
    sbool bMutLocked = 0;

    DEFiRet;

    if (pData->errorFile == NULL) {
        DBGPRINTF(
            "omhttp: no local error logger defined - "
            "ignoring REST error information\n");
        FINALIZE;
    }

    pthread_mutex_lock(&pData->mutErrFile);
    bMutLocked = 1;

    CHKiRet(renderJsonErrorMessage(pWrkrData, reqmsg, &rendered));

    if (pData->fdErrFile == -1) {
        pData->fdErrFile = open((char *)pData->errorFile, O_WRONLY | O_CREAT | O_APPEND | O_LARGEFILE | O_CLOEXEC,
                                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (pData->fdErrFile == -1) {
            LogError(errno, RS_RET_ERR, "omhttp: error opening error file %s", pData->errorFile);
            ABORT_FINALIZE(RS_RET_ERR);
        }
    }

    /* we do not do real error-handling on the err file, as this finally complicates
     * things way to much.
     */
    DBGPRINTF("omhttp: error record: '%s'\n", rendered);
    toWrite = strlen(rendered) + 1;
    /* Note: we overwrite the '\0' terminator with '\n' -- so we avoid
     * caling malloc() -- write() does NOT need '\0'!
     */
    rendered[toWrite - 1] = '\n'; /* NO LONGER A STRING! */
    wrRet = write(pData->fdErrFile, rendered, toWrite);
    if (wrRet != (ssize_t)toWrite) {
        LogError(errno, RS_RET_IO_ERROR, "omhttp: error writing error file %s, write returned %lld", pData->errorFile,
                 (long long)wrRet);
    }

finalize_it:
    if (bMutLocked) pthread_mutex_unlock(&pData->mutErrFile);
    free(rendered);
    RETiRet;
}

static rsRetVal msgAddResponseMetadata(smsg_t *const __restrict__ pMsg,
                                       wrkrInstanceData_t *const pWrkrData,
                                       size_t batch_index) {
    struct json_object *json = NULL;
    DEFiRet;
    CHKmalloc(json = json_object_new_object());
    /*
        Following metadata is exposed:
        $!omhttp!response!code
        $!omhttp!response!body
        $!omhttp!response!batch_index
    */
    json_object_object_add(json, "code", json_object_new_int(pWrkrData->httpStatusCode));
    if (pWrkrData->reply) {
        json_object_object_add(json, "body", json_object_new_string(pWrkrData->reply));
    }
    json_object_object_add(json, "batch_index", json_object_new_int(batch_index));
    CHKiRet(msgAddJSON(pMsg, (uchar *)"!omhttp!response", json, 0, 0));

    /* TODO: possible future, an option to automatically parse to json?
        would be under:
        $!omhttp!response!parsed
    */

finalize_it:
    if (iRet != RS_RET_OK && json) {
        json_object_put(json);
    }
    RETiRet;
}

static rsRetVal queueBatchOnRetryRuleset(wrkrInstanceData_t *const pWrkrData, instanceData *const pData) {
    uchar *msgData;
    smsg_t *pMsg;
    DEFiRet;

    int indexStats = pData->statsBySenders ? pWrkrData->serverIndex : 0;

    if (pData->retryRuleset == NULL) {
        LogError(0, RS_RET_ERR, "omhttp: queueBatchOnRetryRuleset invalid call with a NULL retryRuleset");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    for (size_t i = 0; i < pWrkrData->batch.nmemb; i++) {
        msgData = pWrkrData->batch.data[i];
        DBGPRINTF("omhttp: queueBatchOnRetryRuleset putting message '%s' into retry ruleset '%s'\n", msgData,
                  pData->retryRulesetName);

        // Construct the message object
        CHKiRet(msgConstruct(&pMsg));
        CHKiRet(MsgSetFlowControlType(pMsg, eFLOWCTL_FULL_DELAY));
        MsgSetInputName(pMsg, pInputName);
        MsgSetRawMsg(pMsg, (const char *)msgData, ustrlen(msgData));
        MsgSetMSGoffs(pMsg, 0);  // No header
        MsgSetTAG(pMsg, (const uchar *)"omhttp-retry", 12);

        // And place it on the retry ruleset
        MsgSetRuleset(pMsg, pData->retryRuleset);

        // Add response specific metadata
        if (pData->retryAddMetadata) {
            CHKiRet(msgAddResponseMetadata(pMsg, pWrkrData, i));
        }

        ratelimitAddMsg(pData->ratelimiter, NULL, pMsg);

        // Count here in case not entire batch succeeds
        STATSCOUNTER_INC(pWrkrData->pData->listObjStats[indexStats].ctrMessagesRetry,
                         pWrkrData->pData->listObjStats[indexStats].mutCtrMessagesRetry);
    }
finalize_it:
    RETiRet;
}

static rsRetVal checkResult(wrkrInstanceData_t *pWrkrData, uchar *reqmsg) {
    instanceData *pData;
    long statusCode;
    size_t numMessages;
    DEFiRet;
    CURLcode resCurl = 0;
    int indexStats = 0;

    pData = pWrkrData->pData;
    statusCode = pWrkrData->httpStatusCode;
    indexStats = pData->statsBySenders ? pWrkrData->serverIndex : 0;
    targetStats_t *const serverStats = &pData->listObjStats[indexStats];

    if (pData->batchMode) {
        numMessages = pWrkrData->batch.nmemb;
    } else {
        numMessages = 1;
    }

    /* HTTP status code handling according to new semantics:
     * - 0xx: Transport/connection errors -> retriable
     * - 1xx/2xx: Success
     * - 3xx: Redirection -> non-retriable (for now, redirect support can be added later)
     * - 4xx: Client errors -> permanent failure (non-retriable)
     * - 5xx: Server errors -> retriable
     */
    if (statusCode == 0) {
        // Transport/connection failure - retriable
        STATSCOUNTER_ADD(serverStats->ctrMessagesFail, serverStats->mutCtrMessagesFail, numMessages);
        STATSCOUNTER_INC(serverStats->ctrHttpRequestsStatus0xx, serverStats->mutCtrHttpRequestsStatus0xx);
        iRet = RS_RET_SUSPENDED;
    } else if (statusCode >= 100 && statusCode < 300) {
        // 1xx (informational) and 2xx (success) - treat as success
        STATSCOUNTER_INC(serverStats->ctrHttpStatusSuccess, serverStats->mutCtrHttpStatusSuccess);
        STATSCOUNTER_ADD(serverStats->ctrMessagesSuccess, serverStats->mutCtrMessagesSuccess, numMessages);

        if (statusCode >= 100 && statusCode < 200) {
            STATSCOUNTER_INC(serverStats->ctrHttpRequestsStatus1xx, serverStats->mutCtrHttpRequestsStatus1xx);
        } else if (statusCode >= 200 && statusCode < 300) {
            STATSCOUNTER_INC(serverStats->ctrHttpRequestsStatus2xx, serverStats->mutCtrHttpRequestsStatus2xx);
        }
        iRet = RS_RET_OK;
    } else if (statusCode >= 300 && statusCode < 400) {
        // 3xx - redirection, treat as permanent failure (non-retriable)
        STATSCOUNTER_INC(serverStats->ctrHttpStatusFail, serverStats->mutCtrHttpStatusFail);
        STATSCOUNTER_ADD(serverStats->ctrMessagesFail, serverStats->mutCtrMessagesFail, numMessages);
        STATSCOUNTER_INC(serverStats->ctrHttpRequestsStatus3xx, serverStats->mutCtrHttpRequestsStatus3xx);
        iRet = RS_RET_DATAFAIL;  // permanent failure
    } else if (statusCode >= 400 && statusCode < 500) {
        // 4xx - client error, permanent failure (non-retriable)
        STATSCOUNTER_INC(serverStats->ctrHttpStatusFail, serverStats->mutCtrHttpStatusFail);
        STATSCOUNTER_ADD(serverStats->ctrMessagesFail, serverStats->mutCtrMessagesFail, numMessages);
        STATSCOUNTER_INC(serverStats->ctrHttpRequestsStatus4xx, serverStats->mutCtrHttpRequestsStatus4xx);
        iRet = RS_RET_DATAFAIL;  // permanent failure
    } else if (statusCode >= 500) {
        // 5xx - server error, retriable
        STATSCOUNTER_INC(serverStats->ctrHttpStatusFail, serverStats->mutCtrHttpStatusFail);
        STATSCOUNTER_ADD(serverStats->ctrMessagesFail, serverStats->mutCtrMessagesFail, numMessages);
        STATSCOUNTER_INC(serverStats->ctrHttpRequestsStatus5xx, serverStats->mutCtrHttpRequestsStatus5xx);
        iRet = RS_RET_SUSPENDED;
    } else {
        // Unexpected status code
        STATSCOUNTER_INC(serverStats->ctrHttpStatusFail, serverStats->mutCtrHttpStatusFail);
        STATSCOUNTER_ADD(serverStats->ctrMessagesFail, serverStats->mutCtrMessagesFail, numMessages);
        iRet = RS_RET_DATAFAIL;
    }

    // get curl stats for instance
    {
        long req = 0;
        double total = 0;
        /* record total bytes */
        resCurl = curl_easy_getinfo(pWrkrData->curlPostHandle, CURLINFO_REQUEST_SIZE, &req);
        if (!resCurl) {
            STATSCOUNTER_ADD(serverStats->httpRequestsBytes, serverStats->mutHttpRequestsBytes, (uint64_t)req);
        }
        resCurl = curl_easy_getinfo(pWrkrData->curlPostHandle, CURLINFO_TOTAL_TIME, &total);
        if (CURLE_OK == resCurl) {
            /* this needs to be converted to milliseconds */
            long total_time_ms = (long)(total * 1000);
            STATSCOUNTER_ADD(serverStats->httpRequestsTimeMs, serverStats->mutHttpRequestsTimeMs,
                             (uint64_t)total_time_ms);
        }
    }

    /* Check custom retry codes if configured, overriding default behavior */
    if (pData->nhttpRetryCodes > 0) {
        sbool bMatch = 0;
        for (int i = 0; i < pData->nhttpRetryCodes && pData->httpRetryCodes[i] != 0; ++i) {
            if (statusCode == (long)pData->httpRetryCodes[i]) {
                bMatch = 1;
                break;
            }
        }
        if (bMatch) {
            /* Force retry for explicitly configured codes */
            iRet = RS_RET_SUSPENDED;
        }
    }

    // also check if we can mark this as processed
    if (iRet != RS_RET_OK && pData->ignorableCodes) {
        for (int i = 0; i < pData->nIgnorableCodes && pData->ignorableCodes[i] != 0; ++i) {
            if (statusCode == (long)pData->ignorableCodes[i]) {
                iRet = RS_RET_OK;
                break;
            }
        }
    }

    if (iRet != RS_RET_OK) {
        LogMsg(0, iRet, LOG_ERR, "omhttp: checkResult error http status code: %ld reply: %s", statusCode,
               pWrkrData->reply != NULL ? pWrkrData->reply : "NULL");

        writeDataError(pWrkrData, pWrkrData->pData, reqmsg);

        if (iRet == RS_RET_DATAFAIL) {
            /* Permanent failure - don't retry */
            ABORT_FINALIZE(iRet);
        }

        /* Handle retries */
        if (pData->batchMode && pData->maxBatchSize > 1) {
            /* Batch mode: check if retry ruleset is configured */
            if (pData->retryFailures && pData->retryRuleset != NULL) {
                /* Use retry ruleset for batch retry (legacy/advanced mode) */
                rsRetVal retryRet = queueBatchOnRetryRuleset(pWrkrData, pData);
                if (retryRet != RS_RET_OK) {
                    LogMsg(0, retryRet, LOG_ERR,
                           "omhttp: checkResult error while queueing to retry ruleset - "
                           "some messages may be lost");
                }
                /* Don't suspend entire action - we handled individual messages */
                iRet = RS_RET_OK;
            } else {
                /* No retry ruleset - use core retry by returning RS_RET_SUSPENDED */
                /* This is the new default behavior */
                DBGPRINTF("omhttp: batch failed, using core retry mechanism\n");
                /* iRet already set to RS_RET_SUSPENDED */
            }
        } else {
            /* Non-batch mode: use core retry (RS_RET_SUSPENDED already set) */
            DBGPRINTF("omhttp: single message failed, using core retry mechanism\n");
        }
    }

finalize_it:
    RETiRet;
}

/* Compress a buffer before sending using zlib. Based on code from tools/omfwd.c
 * Initialize the zstrm object for gzip compression, using this init function.
 * deflateInit2(z_stream strm, int level, int method,
 *                             int windowBits, int memLevel, int strategy);
 * strm: the zlib stream held in pWrkrData
 * level: the compression level held in pData
 * method: the operation constant Z_DEFLATED
 * windowBits: the size of the compression window 15 = log_2(32768)
 *     to configure as gzip add 16 to windowBits (w | 16) for final value 31
 * memLevel: the memory optimization level 8 is default)
 * strategy: using Z_DEFAULT_STRATEGY is default
 */
static rsRetVal compressHttpPayload(wrkrInstanceData_t *pWrkrData, uchar *message, unsigned len) {
    int zRet;
    unsigned outavail;
    uchar zipBuf[32 * 1024];

    DEFiRet;

    if (!pWrkrData->bzInitDone) {
        pWrkrData->zstrm.zalloc = Z_NULL;
        pWrkrData->zstrm.zfree = Z_NULL;
        pWrkrData->zstrm.opaque = Z_NULL;
        zRet =
            deflateInit2(&pWrkrData->zstrm, pWrkrData->pData->compressionLevel, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
        if (zRet != Z_OK) {
            DBGPRINTF("omhttp: compressHttpPayload error %d returned from zlib/deflateInit2()\n", zRet);
            ABORT_FINALIZE(RS_RET_ZLIB_ERR);
        }
        pWrkrData->bzInitDone = 1;
    }

    CHKiRet(resetCompressCtx(pWrkrData, len));

    /* now doing the compression */
    pWrkrData->zstrm.next_in = (Bytef *)message;
    pWrkrData->zstrm.avail_in = len;
    /* run deflate() on buffer until everything has been compressed */
    do {
        DBGPRINTF("omhttp: compressHttpPayload in deflate() loop, avail_in %d, total_in %ld\n",
                  pWrkrData->zstrm.avail_in, pWrkrData->zstrm.total_in);
        pWrkrData->zstrm.avail_out = sizeof(zipBuf);
        pWrkrData->zstrm.next_out = zipBuf;

        zRet = deflate(&pWrkrData->zstrm, Z_NO_FLUSH);
        DBGPRINTF("omhttp: compressHttpPayload after deflate, ret %d, avail_out %d\n", zRet,
                  pWrkrData->zstrm.avail_out);
        if (zRet != Z_OK) ABORT_FINALIZE(RS_RET_ZLIB_ERR);
        outavail = sizeof(zipBuf) - pWrkrData->zstrm.avail_out;
        if (outavail != 0) CHKiRet(appendCompressCtx(pWrkrData, zipBuf, outavail));

    } while (pWrkrData->zstrm.avail_out == 0);

    /* run deflate again with Z_FINISH with no new input */
    pWrkrData->zstrm.avail_in = 0;
    do {
        pWrkrData->zstrm.avail_out = sizeof(zipBuf);
        pWrkrData->zstrm.next_out = zipBuf;
        deflate(&pWrkrData->zstrm, Z_FINISH); /* returns Z_STREAM_END == 1 */
        outavail = sizeof(zipBuf) - pWrkrData->zstrm.avail_out;
        if (outavail != 0) CHKiRet(appendCompressCtx(pWrkrData, zipBuf, outavail));

    } while (pWrkrData->zstrm.avail_out == 0);

finalize_it:
    if (pWrkrData->bzInitDone) deflateEnd(&pWrkrData->zstrm);
    pWrkrData->bzInitDone = 0;
    RETiRet;
}

static void ATTR_NONNULL() initCompressCtx(wrkrInstanceData_t *pWrkrData) {
    pWrkrData->compressCtx.buf = NULL;
    pWrkrData->compressCtx.curLen = 0;
    pWrkrData->compressCtx.len = 0;
}

static void ATTR_NONNULL() freeCompressCtx(wrkrInstanceData_t *pWrkrData) {
    if (pWrkrData->compressCtx.buf != NULL) {
        free(pWrkrData->compressCtx.buf);
        pWrkrData->compressCtx.buf = NULL;
    }
}


static rsRetVal ATTR_NONNULL() resetCompressCtx(wrkrInstanceData_t *pWrkrData, size_t len) {
    DEFiRet;
    pWrkrData->compressCtx.curLen = 0;
    pWrkrData->compressCtx.len = len;
    CHKiRet(growCompressCtx(pWrkrData, len));

finalize_it:
    if (iRet != RS_RET_OK) freeCompressCtx(pWrkrData);
    RETiRet;
}

static rsRetVal ATTR_NONNULL() growCompressCtx(wrkrInstanceData_t *pWrkrData, size_t newLen) {
    DEFiRet;
    if (pWrkrData->compressCtx.buf == NULL) {
        CHKmalloc(pWrkrData->compressCtx.buf = (uchar *)malloc(sizeof(uchar) * newLen));
    } else {
        uchar *const newbuf = (uchar *)realloc(pWrkrData->compressCtx.buf, sizeof(uchar) * newLen);
        CHKmalloc(newbuf);
        pWrkrData->compressCtx.buf = newbuf;
    }
    pWrkrData->compressCtx.len = newLen;
finalize_it:
    RETiRet;
}

static rsRetVal ATTR_NONNULL() appendCompressCtx(wrkrInstanceData_t *pWrkrData, uchar *srcBuf, size_t srcLen) {
    size_t newLen;
    DEFiRet;
    newLen = pWrkrData->compressCtx.curLen + srcLen;
    if (newLen > pWrkrData->compressCtx.len) CHKiRet(growCompressCtx(pWrkrData, newLen));

    memcpy(pWrkrData->compressCtx.buf + pWrkrData->compressCtx.curLen, srcBuf, srcLen);
    pWrkrData->compressCtx.curLen = newLen;
finalize_it:
    if (iRet != RS_RET_OK) freeCompressCtx(pWrkrData);
    RETiRet;
}

/* Some duplicate code to curlSetup, but we need to add the gzip content-encoding
 * header at runtime, and if the compression fails, we do not want to send it.
 * Additionally, the curlCheckConnHandle should not be configured with a gzip header.
 */
static rsRetVal ATTR_NONNULL() buildCurlHeaders(wrkrInstanceData_t *pWrkrData, sbool contentEncodeGzip) {
    struct curl_slist *slist = NULL;

    DEFiRet;

    if (pWrkrData->pData->httpcontenttype != NULL) {
        // If content type specified use it, otherwise use a sane default
        slist = curl_slist_append(slist, (char *)pWrkrData->pData->headerContentTypeBuf);
    } else {
        if (pWrkrData->pData->batchMode) {
            // If in batch mode, use the approprate content type header for the format,
            // defaulting to text/plain with newline
            switch (pWrkrData->pData->batchFormat) {
                case FMT_JSONARRAY:
                    slist = curl_slist_append(slist, HTTP_HEADER_CONTENT_JSON);
                    break;
                case FMT_KAFKAREST:
                    slist = curl_slist_append(slist, HTTP_HEADER_CONTENT_KAFKA);
                    break;
                case FMT_NEWLINE:
                    slist = curl_slist_append(slist, HTTP_HEADER_CONTENT_TEXT);
                    break;
                case FMT_LOKIREST:
                    slist = curl_slist_append(slist, HTTP_HEADER_CONTENT_JSON);
                    break;
                default:
                    slist = curl_slist_append(slist, HTTP_HEADER_CONTENT_TEXT);
            }
        } else {
            // Otherwise non batch, presume most users are sending JSON
            slist = curl_slist_append(slist, HTTP_HEADER_CONTENT_JSON);
        }
    }

    CHKmalloc(slist);

    // Configured headers..
    if (pWrkrData->pData->headerBuf != NULL) {
        slist = curl_slist_append(slist, (char *)pWrkrData->pData->headerBuf);
        CHKmalloc(slist);
    }

    for (int k = 0; k < pWrkrData->pData->nHttpHeaders; k++) {
        slist = curl_slist_append(slist, (char *)pWrkrData->pData->httpHeaders[k]);
        CHKmalloc(slist);
    }

    // When sending more than 1Kb, libcurl automatically sends an Except: 100-Continue header
    // and will wait 1s for a response, could make this configurable but for now disable
    slist = curl_slist_append(slist, HTTP_HEADER_EXPECT_EMPTY);
    CHKmalloc(slist);

    if (contentEncodeGzip) {
        slist = curl_slist_append(slist, HTTP_HEADER_ENCODING_GZIP);
        CHKmalloc(slist);
    }

    if (pWrkrData->curlHeader != NULL) curl_slist_free_all(pWrkrData->curlHeader);

    pWrkrData->curlHeader = slist;

finalize_it:
    if (iRet != RS_RET_OK) {
        curl_slist_free_all(slist);
        LogError(0, iRet, "omhttp: error allocating curl header slist, using previous one");
    }
    RETiRet;
}


static rsRetVal ATTR_NONNULL(1, 2) curlPost(
    wrkrInstanceData_t *pWrkrData, uchar *message, int msglen, uchar **tpls, const int nmsgs __attribute__((unused))) {
    CURLcode curlCode;
    CURL *const curl = pWrkrData->curlPostHandle;
    char errbuf[CURL_ERROR_SIZE] = "";
    int indexStats = pWrkrData->pData->statsBySenders ? pWrkrData->serverIndex : 0;
    char *postData;
    int postLen;
    sbool compressed;
    DEFiRet;

    PTR_ASSERT_SET_TYPE(pWrkrData, WRKR_DATA_TYPE_ES);

    if (pWrkrData->pData->numServers > 1) {
        /* needs to be called to support ES HA feature */
        CHKiRet(checkConn(pWrkrData));
    }
    CHKiRet(setPostURL(pWrkrData, tpls));

    pWrkrData->reply = NULL;
    pWrkrData->replyLen = 0;
    pWrkrData->httpStatusCode = 0;

    postData = (char *)message;
    postLen = msglen;
    compressed = 0;

    if (pWrkrData->pData->compress) {
        iRet = compressHttpPayload(pWrkrData, message, msglen);
        if (iRet != RS_RET_OK) {
            LogError(0, iRet, "omhttp: curlPost error while compressing, will default to uncompressed");
        } else {
            postData = (char *)pWrkrData->compressCtx.buf;
            postLen = pWrkrData->compressCtx.curLen;
            compressed = 1;
            DBGPRINTF("omhttp: curlPost compressed %d to %d bytes\n", msglen, postLen);
        }
    }

    buildCurlHeaders(pWrkrData, compressed);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, postLen);
    curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_HTTPHEADER, pWrkrData->curlHeader);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

    curlCode = curl_easy_perform(curl);
    DBGPRINTF("omhttp: curlPost curl returned %lld\n", (long long)curlCode);
    STATSCOUNTER_INC(pWrkrData->pData->listObjStats[indexStats].ctrHttpRequestCount,
                     pWrkrData->pData->listObjStats[indexStats].mutCtrHttpRequestCount);
    STATSCOUNTER_INC(pWrkrData->pData->listObjStats[indexStats].ctrHttpRequestsCount,
                     pWrkrData->pData->listObjStats[indexStats].mutCtrHttpRequestsCount);

    if (curlCode != CURLE_OK) {
        STATSCOUNTER_INC(pWrkrData->pData->listObjStats[indexStats].ctrHttpRequestFail,
                         pWrkrData->pData->listObjStats[indexStats].mutCtrHttpRequestFail);
        LogError(0, RS_RET_SUSPENDED, "omhttp: suspending ourselves due to server failure %lld: %s",
                 (long long)curlCode, errbuf);
        // Check the result here too and retry if needed, then we should suspend
        // Usually in batch mode we clobber any iRet values, but probably not a great
        // idea to keep hitting a dead server. The http status code will be 0 at this point.
        checkResult(pWrkrData, message);
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    } else {
        STATSCOUNTER_INC(pWrkrData->pData->listObjStats[indexStats].ctrHttpRequestSuccess,
                         pWrkrData->pData->listObjStats[indexStats].mutCtrHttpRequestSuccess);
    }

    // Grab the HTTP Response code
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &pWrkrData->httpStatusCode);
    if (pWrkrData->reply == NULL) {
        DBGPRINTF("omhttp: curlPost pWrkrData reply==NULL, replyLen = '%d'\n", pWrkrData->replyLen);
    } else {
        DBGPRINTF("omhttp: curlPost pWrkrData replyLen = '%d'\n", pWrkrData->replyLen);
        if (pWrkrData->replyLen > 0) {
            pWrkrData->reply[pWrkrData->replyLen] = '\0';
            /* Append 0 Byte if replyLen is above 0 - byte has been reserved in malloc */
        }
        // TODO: replyLen++? because 0 Byte is appended
        DBGPRINTF("omhttp: curlPost pWrkrData reply: '%s'\n", pWrkrData->reply);
    }
    CHKiRet(checkResult(pWrkrData, message));

finalize_it:
    incrementServerIndex(pWrkrData);
    if (pWrkrData->reply != NULL) {
        free(pWrkrData->reply);
        pWrkrData->reply = NULL; /* don't leave dangling pointer */
    }
    RETiRet;
}

/* Build a JSON batch that conforms to the Kafka Rest Proxy format.
 * See https://docs.confluent.io/current/kafka-rest/docs/quickstart.html for more info.
 * Want {"records": [{"value": "message1"}, {"value": "message2"}]}
 */
static rsRetVal serializeBatchKafkaRest(wrkrInstanceData_t *pWrkrData, char **batchBuf) {
    fjson_object *batchArray = NULL;
    fjson_object *recordObj = NULL;
    fjson_object *valueObj = NULL;
    fjson_object *msgObj = NULL;

    size_t numMessages = pWrkrData->batch.nmemb;
    size_t sizeTotal = pWrkrData->batch.sizeBytes + numMessages + 1;  // messages + brackets + commas
    DBGPRINTF("omhttp: serializeBatchKafkaRest numMessages=%zd sizeTotal=%zd\n", numMessages, sizeTotal);

    DEFiRet;

    batchArray = fjson_object_new_array();
    if (batchArray == NULL) {
        LogError(0, RS_RET_ERR, "omhttp: serializeBatchKafkaRest failed to create array");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    for (size_t i = 0; i < numMessages; i++) {
        valueObj = fjson_object_new_object();
        if (valueObj == NULL) {
            fjson_object_put(batchArray);  // cleanup
            LogError(0, RS_RET_ERR, "omhttp: serializeBatchKafkaRest failed to create value object");
            ABORT_FINALIZE(RS_RET_ERR);
        }

        msgObj = fjson_tokener_parse((char *)pWrkrData->batch.data[i]);
        if (msgObj == NULL) {
            LogError(0, NO_ERRCODE, "omhttp: serializeBatchKafkaRest failed to parse %s as json ignoring it",
                     pWrkrData->batch.data[i]);
            continue;
        }
        fjson_object_object_add(valueObj, "value", msgObj);
        fjson_object_array_add(batchArray, valueObj);
    }

    recordObj = fjson_object_new_object();
    if (recordObj == NULL) {
        fjson_object_put(batchArray);  // cleanup
        LogError(0, RS_RET_ERR, "omhttp: serializeBatchKafkaRest failed to create record object");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    fjson_object_object_add(recordObj, "records", batchArray);

    const char *batchString = fjson_object_to_json_string_ext(recordObj, FJSON_TO_STRING_PLAIN);
    *batchBuf = strndup(batchString, strlen(batchString));

finalize_it:
    if (recordObj != NULL) {
        fjson_object_put(recordObj);
        recordObj = NULL;
    }

    RETiRet;
}

static rsRetVal serializeBatchLokiRest(wrkrInstanceData_t *pWrkrData, char **batchBuf) {
    fjson_object *batchArray = NULL;
    fjson_object *recordObj = NULL;
    fjson_object *msgObj = NULL;

    size_t numMessages = pWrkrData->batch.nmemb;
    size_t sizeTotal = pWrkrData->batch.sizeBytes + numMessages + 1;  // messages + brackets + commas
    DBGPRINTF("omhttp: serializeBatchLokiRest numMessages=%zd sizeTotal=%zd\n", numMessages, sizeTotal);

    DEFiRet;

    batchArray = fjson_object_new_array();
    if (batchArray == NULL) {
        LogError(0, RS_RET_ERR, "omhttp: serializeBatchLokiRest failed to create array");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    for (size_t i = 0; i < numMessages; i++) {
        DBGPRINTF("omhttp: serializeBatchLokiRest parsing message [%s]\n", (char *)pWrkrData->batch.data[i]);
        msgObj = fjson_tokener_parse((char *)pWrkrData->batch.data[i]);
        if (msgObj == NULL) {
            LogError(0, NO_ERRCODE, "omhttp: serializeBatchLokiRest failed to parse %s as json ignoring it",
                     pWrkrData->batch.data[i]);
            continue;
        }
        fjson_object_array_add(batchArray, msgObj);
    }

    recordObj = fjson_object_new_object();
    if (recordObj == NULL) {
        fjson_object_put(batchArray);  // cleanup
        LogError(0, RS_RET_ERR, "omhttp: serializeBatchLokiRest failed to create record object");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    fjson_object_object_add(recordObj, "streams", batchArray);

    const char *batchString = fjson_object_to_json_string_ext(recordObj, FJSON_TO_STRING_PLAIN);
    *batchBuf = strndup(batchString, strlen(batchString));

finalize_it:
    if (recordObj != NULL) {
        fjson_object_put(recordObj);
        recordObj = NULL;
    }

    RETiRet;
}
/* Build a JSON batch by placing each element in an array.
 */
static rsRetVal serializeBatchJsonArray(wrkrInstanceData_t *pWrkrData, char **batchBuf) {
    fjson_object *batchArray = NULL;
    fjson_object *msgObj = NULL;
    size_t numMessages = pWrkrData->batch.nmemb;
    size_t sizeTotal = pWrkrData->batch.sizeBytes + numMessages + 1;  // messages + brackets + commas
    DBGPRINTF("omhttp: serializeBatchJsonArray numMessages=%zd sizeTotal=%zd\n", numMessages, sizeTotal);

    DEFiRet;

    batchArray = fjson_object_new_array();
    if (batchArray == NULL) {
        LogError(0, RS_RET_ERR, "omhttp: serializeBatchJsonArray failed to create array");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    for (size_t i = 0; i < numMessages; i++) {
        msgObj = fjson_tokener_parse((char *)pWrkrData->batch.data[i]);
        if (msgObj == NULL) {
            LogError(0, NO_ERRCODE, "omhttp: serializeBatchJsonArray failed to parse %s as json, ignoring it",
                     pWrkrData->batch.data[i]);
            continue;
        }
        fjson_object_array_add(batchArray, msgObj);
    }

    const char *batchString = fjson_object_to_json_string_ext(batchArray, FJSON_TO_STRING_PLAIN);
    *batchBuf = strndup(batchString, strlen(batchString));

finalize_it:
    if (batchArray != NULL) {
        fjson_object_put(batchArray);
        batchArray = NULL;
    }
    RETiRet;
}

/* Build a batch by joining each element with a newline character.
 */
static rsRetVal serializeBatchNewline(wrkrInstanceData_t *pWrkrData, char **batchBuf) {
    DEFiRet;
    size_t numMessages = pWrkrData->batch.nmemb;
    size_t sizeTotal = pWrkrData->batch.sizeBytes + numMessages;  // message + newline + null term
    int r = 0;

    DBGPRINTF("omhttp: serializeBatchNewline numMessages=%zd sizeTotal=%zd\n", numMessages, sizeTotal);

    es_str_t *batchString = es_newStr(1024);

    if (batchString == NULL) ABORT_FINALIZE(RS_RET_ERR);

    for (size_t i = 0; i < numMessages; i++) {
        size_t nToCopy = ustrlen(pWrkrData->batch.data[i]);
        if (r == 0) r = es_addBuf(&batchString, (char *)pWrkrData->batch.data[i], nToCopy);
        if (i == numMessages - 1) break;
        if (r == 0) r = es_addChar(&batchString, '\n');
    }

    if (r == 0) *batchBuf = (char *)es_str2cstr(batchString, NULL);

    if (r != 0 || *batchBuf == NULL) {
        LogError(0, RS_RET_ERR, "omhttp: serializeBatchNewline failed to build batch string");
        ABORT_FINALIZE(RS_RET_ERR);
    }

finalize_it:
    if (batchString != NULL) es_deleteStr(batchString);

    RETiRet;
}

/* Return the final batch size in bytes for each serialization method.
 * Used to decide if a batch should be flushed early.
 */
static size_t computeBatchSize(wrkrInstanceData_t *pWrkrData) {
    size_t extraBytes = 0;
    size_t sizeBytes = pWrkrData->batch.sizeBytes;
    size_t numMessages = pWrkrData->batch.nmemb;

    switch (pWrkrData->pData->batchFormat) {
        case FMT_JSONARRAY:
            // square brackets, commas between each message
            // 2 + numMessages - 1 = numMessages + 1
            extraBytes = numMessages > 0 ? numMessages + 1 : 2;
            break;
        case FMT_KAFKAREST:
            // '{}', '[]', '"records":'= 2 + 2 + 10 = 14
            // '{"value":}' for each message = n * 10
            // plus commas between array elements (n > 0 ? n - 1 : 0)
            extraBytes = (numMessages * 10) + 14 + (numMessages > 0 ? numMessages - 1 : 0);
            break;
        case FMT_NEWLINE:
            // newlines between each message
            extraBytes = numMessages > 0 ? numMessages - 1 : 0;
            break;
        case FMT_LOKIREST:
            // {"streams":[ '{}', '[]', '"streams":' = 14
            //    {"stream": {key:value}..., "values":[[timestamp: msg1]]},
            //    {"stream": {key:value}..., "values":[[timestamp: msg2]]}
            // ]}
            // Approximate per-message wrapper overhead (2) plus commas between elements
            extraBytes = (numMessages * 2) + 14 + (numMessages > 0 ? numMessages - 1 : 0);
            break;
        default:
            // newlines between each message
            extraBytes = numMessages > 0 ? numMessages - 1 : 0;
    }

    return sizeBytes + extraBytes + 1;  // plus a null
}

/* Return the delta of extra bytes added by appending one more message to
 * the current batch, based on the configured serialization format.
 */
static inline size_t computeDeltaExtraOnAppend(const wrkrInstanceData_t *pWrkrData) {
    const size_t numMessages = pWrkrData->batch.nmemb;
    switch (pWrkrData->pData->batchFormat) {
        case FMT_JSONARRAY:
            /* add a comma if there is already at least one element */
            return (numMessages > 0) ? 1 : 0;
        case FMT_KAFKAREST:
            /* per-message wrapper overhead (e.g. {"value":}) + comma if needed */
            return 10 + ((numMessages > 0) ? 1 : 0);
        case FMT_NEWLINE:
            /* add a newline if there is already at least one element */
            return (numMessages > 0) ? 1 : 0;
        case FMT_LOKIREST:
            /* approximate per-message overhead in wrapper + comma if needed */
            return 2 + ((numMessages > 0) ? 1 : 0);
        default:
            return (numMessages > 0) ? 1 : 0;
    }
}

static void ATTR_NONNULL() initializeBatch(wrkrInstanceData_t *pWrkrData) {
    pWrkrData->batch.sizeBytes = 0;
    pWrkrData->batch.nmemb = 0;
    if (pWrkrData->batch.restPath != NULL) {
        free(pWrkrData->batch.restPath);
        pWrkrData->batch.restPath = NULL;
    }
}

/* Adds a message to this worker's batch
 */
static rsRetVal buildBatch(wrkrInstanceData_t *pWrkrData, uchar *message) {
    DEFiRet;

    if (pWrkrData->batch.nmemb >= pWrkrData->pData->maxBatchSize) {
        LogError(0, RS_RET_ERR,
                 "omhttp: buildBatch something has gone wrong,"
                 "number of messages in batch is bigger than the max batch size, bailing");
        ABORT_FINALIZE(RS_RET_ERR);
    }
    pWrkrData->batch.data[pWrkrData->batch.nmemb] = message;
    pWrkrData->batch.sizeBytes += strlen((char *)message);
    pWrkrData->batch.nmemb++;

finalize_it:
    RETiRet;
}

static rsRetVal submitBatch(wrkrInstanceData_t *pWrkrData, uchar **tpls) {
    DEFiRet;
    char *batchBuf = NULL;

    switch (pWrkrData->pData->batchFormat) {
        case FMT_JSONARRAY:
            iRet = serializeBatchJsonArray(pWrkrData, &batchBuf);
            break;
        case FMT_KAFKAREST:
            iRet = serializeBatchKafkaRest(pWrkrData, &batchBuf);
            break;
        case FMT_LOKIREST:
            iRet = serializeBatchLokiRest(pWrkrData, &batchBuf);
            break;
        case FMT_NEWLINE:
            iRet = serializeBatchNewline(pWrkrData, &batchBuf);
            break;
        default:
            iRet = serializeBatchNewline(pWrkrData, &batchBuf);
    }

    if (iRet != RS_RET_OK || batchBuf == NULL) ABORT_FINALIZE(iRet);

    DBGPRINTF("omhttp: submitBatch, batch: '%s' tpls: '%p'\n", batchBuf, tpls);

    CHKiRet(curlPost(pWrkrData, (uchar *)batchBuf, strlen(batchBuf), tpls, pWrkrData->batch.nmemb));

finalize_it:
    if (batchBuf != NULL) free(batchBuf);
    RETiRet;
}

BEGINbeginTransaction
    CODESTARTbeginTransaction;
    if (!pWrkrData->pData->batchMode) {
        FINALIZE;
    }

    initializeBatch(pWrkrData);
finalize_it:
ENDbeginTransaction

BEGINcommitTransaction
    unsigned i;
    size_t nBytes;
    sbool submit;
    CODESTARTcommitTransaction;
    instanceData *const pData = pWrkrData->pData;
    const int iNumTpls = pData->dynRestPath ? 2 : 1;
    int indexStats = pWrkrData->pData->statsBySenders ? pWrkrData->serverIndex : 0;


    for (i = 0; i < nParams; ++i) {
        uchar *payload = actParam(pParams, iNumTpls, i, 0).param;
        uchar *tpls[2] = {payload, NULL};
        if (iNumTpls == 2) tpls[1] = actParam(pParams, iNumTpls, i, 1).param;

        STATSCOUNTER_INC(pWrkrData->pData->listObjStats[indexStats].ctrMessagesSubmitted,
                         pWrkrData->pData->listObjStats[indexStats].mutCtrMessagesSubmitted);

        if (pData->batchMode) {
            if (pData->dynRestPath) {
                uchar *restPath = actParam(pParams, iNumTpls, i, 1).param;
                if (pWrkrData->batch.restPath == NULL) {
                    CHKmalloc(pWrkrData->batch.restPath = (uchar *)strdup((char *)restPath));
                } else if (strcmp((char *)pWrkrData->batch.restPath, (char *)restPath) != 0) {
                    /* restPath changed -> flush current batch if it contains data */
                    if (pWrkrData->batch.nmemb > 0) {
                        CHKiRet(submitBatch(pWrkrData, NULL));
                    }
                    initializeBatch(pWrkrData);
                    CHKmalloc(pWrkrData->batch.restPath = (uchar *)strdup((char *)restPath));
                }
            }

            /* If maxBatchSize is 1, immediately build and post a single-element batch */
            if (pData->maxBatchSize == 1) {
                initializeBatch(pWrkrData);
                CHKiRet(buildBatch(pWrkrData, payload));
                CHKiRet(submitBatch(pWrkrData, tpls));
                continue;
            }

            /* Determine if we should submit due to size/bytes thresholds */
            nBytes = ustrlen((char *)payload);
            submit = 0;
            if (pWrkrData->batch.nmemb >= pData->maxBatchSize) {
                submit = 1;
                DBGPRINTF("omhttp: maxbatchsize limit reached submitting batch of %zd elements.\n",
                          pWrkrData->batch.nmemb);
            } else {
                const size_t predicted = computeBatchSize(pWrkrData) + nBytes + computeDeltaExtraOnAppend(pWrkrData);
                if (predicted > pData->maxBatchBytes) {
                    submit = 1;
                    DBGPRINTF("omhttp: maxbytes limit reached submitting partial batch of %zd elements.\n",
                              pWrkrData->batch.nmemb);
                }
            }

            if (submit) {
                /* flush current batch, then start a new one. keep dyn restPath consistent */
                CHKiRet(submitBatch(pWrkrData, NULL));
                initializeBatch(pWrkrData);
                if (pData->dynRestPath) {
                    uchar *restPath = actParam(pParams, iNumTpls, i, 1).param;
                    CHKmalloc(pWrkrData->batch.restPath = (uchar *)strdup((char *)restPath));
                }
            }

            CHKiRet(buildBatch(pWrkrData, payload));
        } else {
            /* non-batch mode: send immediately */
            CHKiRet(curlPost(pWrkrData, payload, strlen((char *)payload), tpls, 1));
        }
    }

    /* finalize any remaining batch data */
    if (pData->batchMode) {
        if (pWrkrData->batch.nmemb > 0) {
            CHKiRet(submitBatch(pWrkrData, NULL));
        } else {
            dbgprintf("omhttp: commitTransaction, pWrkrData->batch.nmemb = 0, nothing to send. \n");
        }
    }
finalize_it:
ENDcommitTransaction


/* Creates authentication header uid:pwd
 */
static rsRetVal computeAuthHeader(char *uid, char *pwd, uchar **authBuf) {
    int r;
    DEFiRet;

    es_str_t *auth = es_newStr(1024);
    if (auth == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "omhttp: failed to allocate es_str auth for auth header construction");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    r = es_addBuf(&auth, uid, strlen(uid));
    if (r == 0) r = es_addChar(&auth, ':');
    if (r == 0 && pwd != NULL) r = es_addBuf(&auth, pwd, strlen(pwd));
    if (r == 0) *authBuf = (uchar *)es_str2cstr(auth, NULL);

    if (r != 0 || *authBuf == NULL) {
        LogError(0, RS_RET_ERR, "omhttp: failed to build auth header\n");
        ABORT_FINALIZE(RS_RET_ERR);
    }

finalize_it:
    if (auth != NULL) es_deleteStr(auth);
    RETiRet;
}

static rsRetVal computeApiHeader(char *key, char *value, uchar **headerBuf) {
    int r;
    DEFiRet;

    es_str_t *header = es_newStr(10240);
    if (header == NULL) {
        LogError(0, RS_RET_OUT_OF_MEMORY, "omhttp: failed to allocate es_str auth for api header construction");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    r = es_addBuf(&header, key, strlen(key));
    if (r == 0) r = es_addChar(&header, ':');
    if (r == 0) r = es_addChar(&header, ' ');
    if (r == 0 && value != NULL) r = es_addBuf(&header, value, strlen(value));
    if (r == 0) *headerBuf = (uchar *)es_str2cstr(header, NULL);

    if (r != 0 || *headerBuf == NULL) {
        LogError(0, RS_RET_ERR, "omhttp: failed to build http header\n");
        ABORT_FINALIZE(RS_RET_ERR);
    }

finalize_it:
    if (header != NULL) es_deleteStr(header);
    RETiRet;
}

static void ATTR_NONNULL() curlSetupCommon(wrkrInstanceData_t *const pWrkrData, CURL *const handle) {
    PTR_ASSERT_SET_TYPE(pWrkrData, WRKR_DATA_TYPE_ES);
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, pWrkrData->curlHeader);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, TRUE);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curlResult);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, pWrkrData);
    if (pWrkrData->pData->proxyHost != NULL) {
        curl_easy_setopt(handle, CURLOPT_PROXY, pWrkrData->pData->proxyHost);
    }
    if (pWrkrData->pData->proxyPort != 0) {
        curl_easy_setopt(handle, CURLOPT_PROXYPORT, pWrkrData->pData->proxyPort);
    }
    if (pWrkrData->pData->restPathTimeout) {
        curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, pWrkrData->pData->restPathTimeout);
    }
    if (pWrkrData->pData->allowUnsignedCerts) curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, FALSE);
    if (pWrkrData->pData->skipVerifyHost) curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, FALSE);
    if (pWrkrData->pData->authBuf != NULL) {
        curl_easy_setopt(handle, CURLOPT_USERPWD, pWrkrData->pData->authBuf);
        curl_easy_setopt(handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    }
    if (pWrkrData->pData->caCertFile) curl_easy_setopt(handle, CURLOPT_CAINFO, pWrkrData->pData->caCertFile);
    if (pWrkrData->pData->myCertFile) curl_easy_setopt(handle, CURLOPT_SSLCERT, pWrkrData->pData->myCertFile);
    if (pWrkrData->pData->myPrivKeyFile) curl_easy_setopt(handle, CURLOPT_SSLKEY, pWrkrData->pData->myPrivKeyFile);
    /* uncomment for in-dept debuggung:
    curl_easy_setopt(handle, CURLOPT_VERBOSE, TRUE); */
}

static void ATTR_NONNULL() curlCheckConnSetup(wrkrInstanceData_t *const pWrkrData) {
    PTR_ASSERT_SET_TYPE(pWrkrData, WRKR_DATA_TYPE_ES);
    curlSetupCommon(pWrkrData, pWrkrData->curlCheckConnHandle);
    curl_easy_setopt(pWrkrData->curlCheckConnHandle, CURLOPT_TIMEOUT_MS, pWrkrData->pData->healthCheckTimeout);
}

static void ATTR_NONNULL(1) curlPostSetup(wrkrInstanceData_t *const pWrkrData) {
    PTR_ASSERT_SET_TYPE(pWrkrData, WRKR_DATA_TYPE_ES);
    curlSetupCommon(pWrkrData, pWrkrData->curlPostHandle);
    curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_POST, 1);
    CURLcode cRet;
    /* Enable TCP keep-alive for this transfer */
    cRet = curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_TCP_KEEPALIVE, 1L);
    if (cRet != CURLE_OK) DBGPRINTF("omhttp: curlPostSetup unknown option CURLOPT_TCP_KEEPALIVE\n");
    /* keep-alive idle time to 120 seconds */
    cRet = curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_TCP_KEEPIDLE, 120L);
    if (cRet != CURLE_OK) DBGPRINTF("omhttp: curlPostSetup unknown option CURLOPT_TCP_KEEPIDLE\n");
    /* interval time between keep-alive probes: 60 seconds */
    cRet = curl_easy_setopt(pWrkrData->curlPostHandle, CURLOPT_TCP_KEEPINTVL, 60L);
    if (cRet != CURLE_OK) DBGPRINTF("omhttp: curlPostSetup unknown option CURLOPT_TCP_KEEPINTVL\n");
}

static rsRetVal ATTR_NONNULL() curlSetup(wrkrInstanceData_t *const pWrkrData) {
    struct curl_slist *slist = NULL;

    DEFiRet;
    if (pWrkrData->pData->httpcontenttype != NULL) {
        slist = curl_slist_append(slist, (char *)pWrkrData->pData->headerContentTypeBuf);
    } else {
        slist = curl_slist_append(slist, HTTP_HEADER_CONTENT_JSON);
    }

    if (pWrkrData->pData->headerBuf != NULL) {
        slist = curl_slist_append(slist, (char *)pWrkrData->pData->headerBuf);
        CHKmalloc(slist);
    }

    for (int k = 0; k < pWrkrData->pData->nHttpHeaders; k++) {
        slist = curl_slist_append(slist, (char *)pWrkrData->pData->httpHeaders[k]);
        CHKmalloc(slist);
    }

    // When sending more than 1Kb, libcurl automatically sends an Except: 100-Continue header
    // and will wait 1s for a response, could make this configurable but for now disable
    slist = curl_slist_append(slist, HTTP_HEADER_EXPECT_EMPTY);
    pWrkrData->curlHeader = slist;
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

static void ATTR_NONNULL() curlCleanup(wrkrInstanceData_t *const pWrkrData) {
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
}

static void ATTR_NONNULL() setInstParamDefaults(instanceData *const pData) {
    pData->serverBaseUrls = NULL;
    pData->defaultPort = 443;
    pData->healthCheckTimeout = 3500;
    pData->uid = NULL;
    pData->restPathTimeout = 0;
    pData->httpcontenttype = NULL;
    pData->headerContentTypeBuf = NULL;
    pData->httpheaderkey = NULL;
    pData->httpheadervalue = NULL;
    pData->httpHeaders = NULL;
    pData->nHttpHeaders = 0;
    pData->pwd = NULL;
    pData->authBuf = NULL;
    pData->headerBuf = NULL;
    pData->restPath = NULL;
    pData->checkPath = NULL;
    pData->dynRestPath = 0;
    pData->proxyHost = NULL;
    pData->proxyPort = 0;
    pData->batchMode = 0;
    pData->batchFormatName = (uchar *)"newline";
    pData->batchFormat = FMT_NEWLINE;
    pData->bFreeBatchFormatName = 0;
    pData->useHttps = 1;
    pData->maxBatchBytes = DEFAULT_MAX_BATCH_BYTES;  // 10 MB - default max message size for AWS API Gateway
    pData->maxBatchSize = 100;  // 100 messages
    pData->compress = 0;  // off
    pData->compressionLevel = -1;  // default compression
    pData->allowUnsignedCerts = 0;
    pData->skipVerifyHost = 0;
    pData->tplName = NULL;
    pData->errorFile = NULL;
    pData->caCertFile = NULL;
    pData->myCertFile = NULL;
    pData->myPrivKeyFile = NULL;
    pData->reloadOnHup = 0;
    pData->retryFailures = 0;
    pData->retryAddMetadata = 0;
    pData->nhttpRetryCodes = 0;
    pData->httpRetryCodes = NULL;
    pData->ratelimitBurst = 20000;
    pData->ratelimitInterval = 600;
    pData->ratelimiter = NULL;
    pData->retryRulesetName = NULL;
    pData->retryRuleset = NULL;
    pData->nIgnorableCodes = 0;
    pData->ignorableCodes = NULL;
    pData->statsBySenders = 0;  // Disable by default
    // increment number of instances
    ++omhttpInstancesCnt;
}

static rsRetVal checkHeaderParam(char *const param) {
    DEFiRet;
    char *val = strstr(param, ":");
    if (val == NULL) {
        LogError(0, RS_RET_PARAM_ERROR,
                 "missing ':' delimiter in "
                 "parameter '%s'",
                 param);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }
finalize_it:
    RETiRet;
}

/* Apply profile settings to instance configuration
 * Profiles are meta-configurations that set multiple parameters at once
 * for common use cases like Loki or Splunk HEC
 */
static rsRetVal applyProfileSettings(instanceData *const pData, const char *const profile) {
    DEFiRet;

    if (strcasecmp(profile, "loki") == 0) {
        /* Loki profile settings */
        LogMsg(0, RS_RET_OK, LOG_INFO, "omhttp: applying 'loki' profile");

        /* Set batch format to lokirest if not already set */
        if (!pData->bFreeBatchFormatName) {
            pData->batchFormatName = (uchar *)"lokirest";
            pData->batchFormat = FMT_LOKIREST;
        }

        /* Set default rest path for Loki if not set */
        if (pData->restPath == NULL) {
            CHKmalloc(pData->restPath = (uchar *)strdup("loki/api/v1/push"));
        }

        /* Enable batch mode by default for Loki */
        if (!pData->batchMode) {
            pData->batchMode = 1;
        }

        /* Enable compression for Loki (typically beneficial) */
        if (!pData->compress) {
            pData->compress = 1;
            pData->compressionLevel = -1; /* default compression */
        }

        /* Set default retry codes to 5xx if not configured */
        if (pData->nhttpRetryCodes == 0) {
            static const unsigned int loki_retry_codes[] = {500, 502, 503, 504};
            const size_t num_codes = sizeof(loki_retry_codes) / sizeof(loki_retry_codes[0]);
            pData->nhttpRetryCodes = num_codes;
            CHKmalloc(pData->httpRetryCodes = malloc(sizeof(loki_retry_codes)));
            memcpy(pData->httpRetryCodes, loki_retry_codes, sizeof(loki_retry_codes));
        }

    } else if (strncasecmp(profile, "hec:", 4) == 0) {
        /* HEC (HTTP Event Collector) profile */
        const char *vendor = profile + 4;

        if (strcasecmp(vendor, "splunk") == 0) {
            LogMsg(0, RS_RET_OK, LOG_INFO, "omhttp: applying 'hec:splunk' profile");

            /* Set default rest path for Splunk HEC */
            if (pData->restPath == NULL) {
                CHKmalloc(pData->restPath = (uchar *)strdup("services/collector/event"));
            }

            /* Set batch format to newline (Splunk HEC uses newline-delimited JSON) */
            if (!pData->bFreeBatchFormatName) {
                pData->batchFormatName = (uchar *)"newline";
                pData->batchFormat = FMT_NEWLINE;
            }

            /* Enable batch mode for HEC */
            if (!pData->batchMode) {
                pData->batchMode = 1;
            }

            /* Set default max batch bytes (Splunk recommends < 1MB) */
            if (pData->maxBatchBytes == DEFAULT_MAX_BATCH_BYTES) { /* still default */
                pData->maxBatchBytes = SPLUNK_HEC_MAX_BATCH_BYTES; /* 1MB */
            }

            /* Note: Authorization header should be set separately with httpheaderkey/value
             * e.g., httpheaderkey="Authorization" httpheadervalue="Splunk YOUR-HEC-TOKEN"
             */

        } else {
            LogError(0, RS_RET_PARAM_ERROR, "omhttp: unknown HEC vendor '%s' in profile", vendor);
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }

    } else {
        LogError(0, RS_RET_PARAM_ERROR, "omhttp: unknown profile '%s'", profile);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

finalize_it:
    RETiRet;
}


static rsRetVal setStatsObject(instanceData *const pData, const char *const serverParam, size_t index) {
    uchar ctrName[256];
    DEFiRet;

    targetStats_t *const serverStats = &pData->listObjStats[index];

    if (serverParam == NULL) {
        snprintf((char *)ctrName, sizeof(ctrName), "%s(ALL)", pData->statsName);
        ctrName[sizeof(ctrName) - 1] = '\0';
    } else {
        snprintf((char *)ctrName, sizeof(ctrName), "%s(%s)", pData->statsName, serverParam);
        ctrName[sizeof(ctrName) - 1] = '\0';
    }

    // instantiate the stats object and add the counters
    CHKiRet(statsobj.Construct(&(serverStats->defaultstats)));
    CHKiRet(statsobj.SetName(serverStats->defaultstats, ctrName));
    CHKiRet(statsobj.SetOrigin(serverStats->defaultstats, (uchar *)"omhttp"));

    STATSCOUNTER_INIT(serverStats->ctrMessagesSubmitted, serverStats->mutCtrMessagesSubmitted);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"messages.submitted", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->ctrMessagesSubmitted)));

    STATSCOUNTER_INIT(serverStats->ctrMessagesSuccess, serverStats->mutCtrMessagesSuccess);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"messages.success", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->ctrMessagesSuccess)));

    STATSCOUNTER_INIT(serverStats->ctrMessagesFail, serverStats->mutCtrMessagesFail);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"messages.fail", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->ctrMessagesFail)));

    STATSCOUNTER_INIT(serverStats->ctrMessagesRetry, serverStats->mutCtrMessagesRetry);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"messages.retry", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->ctrMessagesRetry)));

    STATSCOUNTER_INIT(serverStats->ctrHttpRequestCount, serverStats->mutCtrHttpRequestCount);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"request.count", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->ctrHttpRequestCount)));

    STATSCOUNTER_INIT(serverStats->ctrHttpRequestSuccess, serverStats->mutCtrHttpRequestSuccess);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"request.success", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->ctrHttpRequestSuccess)));

    STATSCOUNTER_INIT(serverStats->ctrHttpRequestFail, serverStats->mutCtrHttpRequestFail);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"request.fail", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(serverStats->ctrHttpRequestFail)));

    STATSCOUNTER_INIT(serverStats->ctrHttpStatusSuccess, serverStats->mutCtrHttpStatusSuccess);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"request.status.success", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->ctrHttpStatusSuccess)));

    STATSCOUNTER_INIT(serverStats->ctrHttpStatusFail, serverStats->mutCtrHttpStatusFail);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"request.status.fail", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->ctrHttpStatusFail)));

    STATSCOUNTER_INIT(serverStats->ctrHttpRequestsCount, serverStats->mutCtrHttpRequestsCount);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"requests.count", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->ctrHttpRequestsCount)));

    STATSCOUNTER_INIT(serverStats->ctrHttpRequestsStatus0xx, serverStats->mutCtrHttpRequestsStatus0xx);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"requests.status.0xx", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->ctrHttpRequestsStatus0xx)));

    STATSCOUNTER_INIT(serverStats->ctrHttpRequestsStatus1xx, serverStats->mutCtrHttpRequestsStatus1xx);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"requests.status.1xx", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->ctrHttpRequestsStatus1xx)));

    STATSCOUNTER_INIT(serverStats->ctrHttpRequestsStatus2xx, serverStats->mutCtrHttpRequestsStatus2xx);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"requests.status.2xx", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->ctrHttpRequestsStatus2xx)));

    STATSCOUNTER_INIT(serverStats->ctrHttpRequestsStatus3xx, serverStats->mutCtrHttpRequestsStatus3xx);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"requests.status.3xx", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->ctrHttpRequestsStatus3xx)));

    STATSCOUNTER_INIT(serverStats->ctrHttpRequestsStatus4xx, serverStats->mutCtrHttpRequestsStatus4xx);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"requests.status.4xx", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->ctrHttpRequestsStatus4xx)));

    STATSCOUNTER_INIT(serverStats->ctrHttpRequestsStatus5xx, serverStats->mutCtrHttpRequestsStatus5xx);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"requests.status.5xx", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->ctrHttpRequestsStatus5xx)));

    STATSCOUNTER_INIT(serverStats->httpRequestsBytes, serverStats->mutHttpRequestsBytes);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"requests.bytes", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->httpRequestsBytes)));

    STATSCOUNTER_INIT(serverStats->httpRequestsTimeMs, serverStats->mutHttpRequestsTimeMs);
    CHKiRet(statsobj.AddCounter(serverStats->defaultstats, (uchar *)"requests.time_ms", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(serverStats->httpRequestsTimeMs)));

    CHKiRet(statsobj.ConstructFinalize(serverStats->defaultstats));

finalize_it:
    RETiRet;
}

BEGINnewActInst
    struct cnfparamvals *pvals;
    char *serverParam = NULL;
    struct cnfarray *servers = NULL;
    char *profileName = NULL;
    int i;
    int iNumTpls;
    FILE *fp;
    char errStr[1024];
    char *batchFormatName;
    int compressionLevel = -1;
    CODESTARTnewActInst;
    if ((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    CHKiRet(createInstance(&pData));
    setInstParamDefaults(pData);

    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(actpblk.descr[i].name, "server")) {
            servers = pvals[i].val.d.ar;
        } else if (!strcmp(actpblk.descr[i].name, "errorfile")) {
            pData->errorFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "serverport")) {
            pData->defaultPort = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "healthchecktimeout")) {
            pData->healthCheckTimeout = (long)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "restpathtimeout")) {
            pData->restPathTimeout = (long)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "uid")) {
            pData->uid = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "httpcontenttype")) {
            pData->httpcontenttype = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "httpheaderkey")) {
            pData->httpheaderkey = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "httpheadervalue")) {
            pData->httpheadervalue = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "httpheaders")) {
            pData->nHttpHeaders = pvals[i].val.d.ar->nmemb;
            CHKmalloc(pData->httpHeaders = malloc(sizeof(uchar *) * pvals[i].val.d.ar->nmemb));
            for (int j = 0; j < pvals[i].val.d.ar->nmemb; ++j) {
                char *cstr = es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
                CHKiRet(checkHeaderParam(cstr));
                pData->httpHeaders[j] = (uchar *)cstr;
            }
        } else if (!strcmp(actpblk.descr[i].name, "pwd")) {
            pData->pwd = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "restpath")) {
            pData->restPath = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "checkpath")) {
            pData->checkPath = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "dynrestpath")) {
            pData->dynRestPath = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "proxyhost")) {
            pData->proxyHost = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "proxyport")) {
            pData->proxyPort = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "batch")) {
            pData->batchMode = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "batch.format")) {
            batchFormatName = es_str2cstr(pvals[i].val.d.estr, NULL);
            if (strstr(VALID_BATCH_FORMATS, batchFormatName) != NULL) {
                pData->batchFormatName = (uchar *)batchFormatName;
                pData->bFreeBatchFormatName = 1;
                if (!strcmp(batchFormatName, "newline")) {
                    pData->batchFormat = FMT_NEWLINE;
                } else if (!strcmp(batchFormatName, "jsonarray")) {
                    pData->batchFormat = FMT_JSONARRAY;
                } else if (!strcmp(batchFormatName, "kafkarest")) {
                    pData->batchFormat = FMT_KAFKAREST;
                } else if (!strcmp(batchFormatName, "lokirest")) {
                    pData->batchFormat = FMT_LOKIREST;
                }
            } else {
                LogError(0, NO_ERRCODE, "error: 'batch.format' %s unknown defaulting to 'newline'", batchFormatName);
            }
        } else if (!strcmp(actpblk.descr[i].name, "batch.maxbytes")) {
            pData->maxBatchBytes = (size_t)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "batch.maxsize")) {
            pData->maxBatchSize = (size_t)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "compress")) {
            pData->compress = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "compress.level")) {
            compressionLevel = pvals[i].val.d.n;
            if (compressionLevel == -1 || (compressionLevel >= 0 && compressionLevel < 10)) {
                pData->compressionLevel = compressionLevel;
            } else {
                LogError(0, NO_ERRCODE,
                         "omhttp: invalid compress.level %d using default instead,"
                         "valid levels are -1 and 0-9",
                         compressionLevel);
            }
        } else if (!strcmp(actpblk.descr[i].name, "allowunsignedcerts")) {
            pData->allowUnsignedCerts = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "skipverifyhost")) {
            pData->skipVerifyHost = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "usehttps")) {
            pData->useHttps = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "template")) {
            pData->tplName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "tls.cacert")) {
            pData->caCertFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            fp = fopen((const char *)pData->caCertFile, "r");
            if (fp == NULL) {
                rs_strerror_r(errno, errStr, sizeof(errStr));
                LogError(0, RS_RET_NO_FILE_ACCESS, "error: 'tls.cacert' file %s couldn't be accessed: %s\n",
                         pData->caCertFile, errStr);
            } else {
                fclose(fp);
            }
        } else if (!strcmp(actpblk.descr[i].name, "tls.mycert")) {
            pData->myCertFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            fp = fopen((const char *)pData->myCertFile, "r");
            if (fp == NULL) {
                rs_strerror_r(errno, errStr, sizeof(errStr));
                LogError(0, RS_RET_NO_FILE_ACCESS, "error: 'tls.mycert' file %s couldn't be accessed: %s\n",
                         pData->myCertFile, errStr);
            } else {
                fclose(fp);
            }
        } else if (!strcmp(actpblk.descr[i].name, "tls.myprivkey")) {
            pData->myPrivKeyFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            fp = fopen((const char *)pData->myPrivKeyFile, "r");
            if (fp == NULL) {
                rs_strerror_r(errno, errStr, sizeof(errStr));
                LogError(0, RS_RET_NO_FILE_ACCESS, "error: 'tls.myprivkey' file %s couldn't be accessed: %s\n",
                         pData->myPrivKeyFile, errStr);
            } else {
                fclose(fp);
            }
        } else if (!strcmp(actpblk.descr[i].name, "reloadonhup")) {
            pData->reloadOnHup = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "httpretrycodes")) {
            pData->nhttpRetryCodes = pvals[i].val.d.ar->nmemb;
            // note: use zero as sentinel value
            CHKmalloc(pData->httpRetryCodes = calloc(pvals[i].val.d.ar->nmemb, sizeof(unsigned int)));
            int count = 0;
            for (int j = 0; j < pvals[i].val.d.ar->nmemb; ++j) {
                int bSuccess = 0;
                long long n = es_str2num(pvals[i].val.d.ar->arr[j], &bSuccess);
                if (!bSuccess) {
                    char *cstr = es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
                    LogError(0, RS_RET_NO_FILE_ACCESS, "error: 'httpRetryCode' '%s' is not a number - ignored\n", cstr);
                    free(cstr);
                } else {
                    pData->httpRetryCodes[count++] = n;
                }
            }
        } else if (!strcmp(actpblk.descr[i].name, "retry")) {
            pData->retryFailures = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "retry.ruleset")) {
            pData->retryRulesetName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "retry.addmetadata")) {
            pData->retryAddMetadata = pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "ratelimit.burst")) {
            pData->ratelimitBurst = (unsigned int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "ratelimit.interval")) {
            pData->ratelimitInterval = (unsigned int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "name")) {
            pData->statsName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "httpignorablecodes")) {
            pData->nIgnorableCodes = pvals[i].val.d.ar->nmemb;
            // note: use zero as sentinel value
            CHKmalloc(pData->ignorableCodes = calloc(pvals[i].val.d.ar->nmemb, sizeof(unsigned int)));
            int count = 0;
            for (int j = 0; j < pvals[i].val.d.ar->nmemb; ++j) {
                int bSuccess = 0;
                long long n = es_str2num(pvals[i].val.d.ar->arr[j], &bSuccess);
                if (!bSuccess) {
                    char *cstr = es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
                    LogError(0, RS_RET_NO_FILE_ACCESS, "error: 'httpIgnorableCodes' '%s' is not a number - ignored\n",
                             cstr);
                    free(cstr);
                } else {
                    pData->ignorableCodes[count++] = n;
                }
            }
        } else if (!strcmp(actpblk.descr[i].name, "profile")) {
            profileName = es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(actpblk.descr[i].name, "statsbysenders")) {
            pData->statsBySenders = pvals[i].val.d.n;
        } else {
            LogError(0, RS_RET_INTERNAL_ERROR,
                     "omhttp: program error, "
                     "non-handled param '%s'",
                     actpblk.descr[i].name);
        }
    }

    if (pData->pwd != NULL && pData->uid == NULL) {
        LogError(0, RS_RET_UID_MISSING,
                 "omhttp: password is provided, but no uid "
                 "- action definition invalid");
        ABORT_FINALIZE(RS_RET_UID_MISSING);
    }
    if (pData->httpheaderkey != NULL && pData->httpheadervalue == NULL) {
        LogError(0, RS_RET_UID_MISSING,
                 "omhttp: http header key is provided, but no http header value "
                 "- action definition invalid");
        ABORT_FINALIZE(RS_RET_UID_MISSING);
    }
    if (pData->dynRestPath && pData->restPath == NULL) {
        LogError(0, RS_RET_CONFIG_ERROR,
                 "omhttp: requested dynamic rest path, but no name for rest "
                 "path template given - action definition invalid");
        ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
    }

    /* Apply profile settings if specified */
    if (profileName != NULL) {
        CHKiRet(applyProfileSettings(pData, profileName));
        free(profileName);
        profileName = NULL;
    }

    if (pData->proxyHost == NULL) {
        const char *http_proxy;
        if ((http_proxy = getenv("http_proxy")) == NULL) {
            http_proxy = getenv("HTTP_PROXY");
        }
        if (http_proxy != NULL) {
            pData->proxyHost = ustrdup(http_proxy);
        }
    }

    if (pData->uid != NULL) CHKiRet(computeAuthHeader((char *)pData->uid, (char *)pData->pwd, &pData->authBuf));
    if (pData->httpcontenttype != NULL)
        CHKiRet(computeApiHeader((char *)"Content-Type", (char *)pData->httpcontenttype, &pData->headerContentTypeBuf));

    if (pData->httpheaderkey != NULL)
        CHKiRet(computeApiHeader((char *)pData->httpheaderkey, (char *)pData->httpheadervalue, &pData->headerBuf));

    iNumTpls = 1;
    if (pData->dynRestPath) ++iNumTpls;
    DBGPRINTF("omhttp: requesting %d templates\n", iNumTpls);
    CODE_STD_STRING_REQUESTnewActInst(iNumTpls);

    CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar *)strdup((pData->tplName == NULL) ? " StdJSONFmt" : (char *)pData->tplName),
                         OMSR_NO_RQD_TPL_OPTS));


    /* we need to request additional templates. If we have a dynamic search index,
     * it will always be string 1. Type may be 1 or 2, depending on whether search
     * index is dynamic as well. Rule needs to be followed throughout the module.
     */
    iNumTpls = 1;
    if (pData->dynRestPath) {
        CHKiRet(OMSRsetEntry(*ppOMSR, iNumTpls, ustrdup(pData->restPath), OMSR_NO_RQD_TPL_OPTS));
        ++iNumTpls;
    }

    if (!pData->statsName) {
        uchar pszAName[64];
        snprintf((char *)pszAName, sizeof(pszAName), "omhttp-%d", omhttpInstancesCnt);
        pData->statsName = ustrdup(pszAName);
    }

    if (servers != NULL) {
        pData->numServers = servers->nmemb;
        pData->serverBaseUrls = malloc(servers->nmemb * sizeof(uchar *));
        if (pData->serverBaseUrls == NULL) {
            LogError(0, RS_RET_ERR,
                     "omhttp: unable to allocate buffer "
                     "for http server configuration.");
            ABORT_FINALIZE(RS_RET_ERR);
        }
        if (pData->statsBySenders) {
            pData->listObjStats = malloc(servers->nmemb * sizeof(targetStats_t));
        } else {
            pData->listObjStats = malloc(1 * sizeof(targetStats_t));
            CHKiRet(setStatsObject(pData, NULL, 0));
        }

        for (i = 0; i < servers->nmemb; ++i) {
            serverParam = es_str2cstr(servers->arr[i], NULL);
            if (serverParam == NULL) {
                LogError(0, RS_RET_ERR,
                         "omhttp: unable to allocate buffer "
                         "for http server configuration.");
                ABORT_FINALIZE(RS_RET_ERR);
            }
            /* Remove a trailing slash if it exists */
            const size_t serverParamLastChar = strlen(serverParam) - 1;
            if (serverParam[serverParamLastChar] == '/') {
                serverParam[serverParamLastChar] = '\0';
            }
            if (pData->statsBySenders) {
                CHKiRet(setStatsObject(pData, serverParam, i));
            }

            CHKiRet(computeBaseUrl(serverParam, pData->defaultPort, pData->useHttps, pData->serverBaseUrls + i));
            free(serverParam);
            serverParam = NULL;
        }
    } else {
        LogMsg(0, RS_RET_OK, LOG_WARNING, "omhttp: No servers specified, using localhost");
        pData->numServers = 1;
        pData->serverBaseUrls = malloc(sizeof(uchar *));
        if (pData->serverBaseUrls == NULL) {
            LogError(0, RS_RET_ERR,
                     "omhttp: unable to allocate buffer "
                     "for http server configuration.");
            ABORT_FINALIZE(RS_RET_ERR);
        }
        CHKiRet(computeBaseUrl("localhost", pData->defaultPort, pData->useHttps, pData->serverBaseUrls));
    }

    if (pData->retryFailures) {
        CHKiRet(ratelimitNew(&pData->ratelimiter, "omhttp", NULL));
        ratelimitSetLinuxLike(pData->ratelimiter, pData->ratelimitInterval, pData->ratelimitBurst);
        ratelimitSetNoTimeCache(pData->ratelimiter);
    }


    /* node created, let's add to list of instance configs for the module */
    if (loadModConf->tail == NULL) {
        loadModConf->tail = loadModConf->root = pData;
    } else {
        loadModConf->tail->next = pData;
        loadModConf->tail = pData;
    }

    CODE_STD_FINALIZERnewActInst;
    cnfparamvalsDestruct(pvals, &actpblk);
    if (serverParam) free(serverParam);
    if (profileName) free(profileName);
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
    instanceConf_t *inst;
    CODESTARTcheckCnf;
    for (inst = pModConf->root; inst != NULL; inst = inst->next) {
        ruleset_t *pRuleset;
        rsRetVal localRet;

        if (inst->retryRulesetName) {
            localRet = ruleset.GetRuleset(pModConf->pConf, &pRuleset, inst->retryRulesetName);
            if (localRet == RS_RET_NOT_FOUND) {
                LogError(0, localRet,
                         "omhttp: retry.ruleset '%s' not found - "
                         "no retry ruleset will be used",
                         inst->retryRulesetName);
            } else {
                inst->retryRuleset = pRuleset;
            }
        }
    }
ENDcheckCnf


BEGINactivateCnf
    CODESTARTactivateCnf;
ENDactivateCnf


BEGINfreeCnf
    CODESTARTfreeCnf;
ENDfreeCnf


// HUP handling for the instance...
BEGINdoHUP
    CODESTARTdoHUP;
    pthread_mutex_lock(&pData->mutErrFile);
    if (pData->fdErrFile != -1) {
        close(pData->fdErrFile);
        pData->fdErrFile = -1;
    }
    pthread_mutex_unlock(&pData->mutErrFile);
ENDdoHUP


// HUP handling for the worker...
BEGINdoHUPWrkr
    CODESTARTdoHUPWrkr;
    if (pWrkrData->pData->reloadOnHup) {
        LogMsg(0, NO_ERRCODE, LOG_INFO, "omhttp: received HUP reloading curl handles");
        curlCleanup(pWrkrData);
        CHKiRet(curlSetup(pWrkrData));
    }
finalize_it:
ENDdoHUPWrkr


BEGINmodExit
    CODESTARTmodExit;
    if (pInputName != NULL) prop.Destruct(&pInputName);
    curl_global_cleanup();
    objRelease(prop, CORE_COMPONENT);
    objRelease(ruleset, CORE_COMPONENT);
    objRelease(statsobj, CORE_COMPONENT);
ENDmodExit

NO_LEGACY_CONF_parseSelectorAct

    BEGINqueryEtryPt CODESTARTqueryEtryPt;
CODEqueryEtryPt_STD_OMODTX_QUERIES CODEqueryEtryPt_STD_OMOD8_QUERIES CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
    CODEqueryEtryPt_doHUP CODEqueryEtryPt_doHUPWrkr /* Load the worker HUP handling code */
    CODEqueryEtryPt_STD_CONF2_QUERIES
ENDqueryEtryPt


BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr CHKiRet(objUse(prop, CORE_COMPONENT));
    CHKiRet(objUse(ruleset, CORE_COMPONENT));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));

    if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
        LogError(0, RS_RET_OBJ_CREATION_FAILED, "CURL fail. -http disabled");
        ABORT_FINALIZE(RS_RET_OBJ_CREATION_FAILED);
    }

    CHKiRet(prop.Construct(&pInputName));
    CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("omhttp"), sizeof("omhttp") - 1));
    CHKiRet(prop.ConstructFinalize(pInputName));
ENDmodInit

/* vi:set ai:
 */
