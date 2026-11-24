/* SPDX-License-Identifier: Apache-2.0 */

/* omkinesis.c - Amazon Kinesis Data Streams PutRecord output
 *
 * Concurrency & Locking:
 * - Action configuration is immutable after newActInst().
 * - Every worker owns its CURL handle; no mutable state is shared between workers.
 *
 * Copyright 2026 Adiscon GmbH. Licensed under the Apache License, Version 2.0.
 */
#include "config.h"
#include "rsyslog.h"
#include <curl/curl.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("omkinesis")
DEF_OMOD_STATIC_DATA;

typedef struct _instanceData {
    uchar *stream;
    uchar *region;
    uchar *partitionKey;
    uchar *partitionKeyTplName;
    uchar *tplName;
    uchar *accessKey;
    uchar *secretKey;
    uchar *sessionToken;
    uchar *endpoint;
    char *url;
    char *sigv4;
    char *userpwd;
    long timeout;
    long maxRecordSize;
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData;
    CURL *curl;
} wrkrInstanceData_t;

typedef struct responseBuf_s {
    char data[4096];
    size_t len;
} responseBuf_t;

static struct cnfparamdescr actpdescr[] = {
    {"stream", eCmdHdlrString, 0},        {"region", eCmdHdlrString, 0},
    {"partition_key", eCmdHdlrString, 0}, {"partition_key_template", eCmdHdlrGetWord, 0},
    {"template", eCmdHdlrGetWord, 0},     {"access_key", eCmdHdlrString, 0},
    {"secret_key", eCmdHdlrString, 0},    {"session_token", eCmdHdlrString, 0},
    {"endpoint", eCmdHdlrString, 0},      {"timeout", eCmdHdlrInt, 0},
    {"max_record_size", eCmdHdlrInt, 0},
};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

struct modConfData_s {
    rsconf_t *pConf;
};
static modConfData_t *runModConf = NULL;

static size_t captureResponse(void *ptr, size_t size, size_t nmemb, void *userp) {
    responseBuf_t *response = userp;
    if (size != 0 && nmemb > SIZE_MAX / size) return 0;
    size_t bytes = size * nmemb;
    size_t available = sizeof(response->data) - response->len - 1;
    size_t copy = bytes < available ? bytes : available;
    memcpy(response->data + response->len, ptr, copy);
    response->len += copy;
    response->data[response->len] = '\0';
    return bytes;
}

static int validRegion(const char *region) {
    if (*region == '\0') return 0;
    for (const unsigned char *p = (const unsigned char *)region; *p != '\0'; ++p) {
        if (!isalnum(*p) && *p != '-') return 0;
    }
    return 1;
}

static int validEndpoint(const char *url) {
    CURLU *parsed = curl_url();
    char *scheme = NULL, *host = NULL, *user = NULL, *password = NULL;
    int valid = 0;
    if (parsed == NULL || curl_url_set(parsed, CURLUPART_URL, url, 0) != CURLUE_OK ||
        curl_url_get(parsed, CURLUPART_SCHEME, &scheme, 0) != CURLUE_OK ||
        curl_url_get(parsed, CURLUPART_HOST, &host, 0) != CURLUE_OK)
        goto done;
    /* Credentials in the authority can conceal the real host. */
    if (curl_url_get(parsed, CURLUPART_USER, &user, 0) == CURLUE_OK ||
        curl_url_get(parsed, CURLUPART_PASSWORD, &password, 0) == CURLUE_OK)
        goto done;
    valid = !strcmp(scheme, "https") ||
            (!strcmp(scheme, "http") && (!strcmp(host, "127.0.0.1") || !strcmp(host, "localhost")));
done:
    curl_free(scheme);
    curl_free(host);
    curl_free(user);
    curl_free(password);
    curl_url_cleanup(parsed);
    return valid;
}

static char *base64Encode(const unsigned char *src, size_t len) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (len > (SIZE_MAX - 1) / 4 * 3) return NULL;
    size_t outLen = ((len + 2) / 3) * 4;
    char *out = malloc(outLen + 1);
    if (out == NULL) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i += 3) {
        unsigned int v = (unsigned int)src[i] << 16;
        if (i + 1 < len) v |= (unsigned int)src[i + 1] << 8;
        if (i + 2 < len) v |= src[i + 2];
        out[j++] = alphabet[(v >> 18) & 63];
        out[j++] = alphabet[(v >> 12) & 63];
        out[j++] = i + 1 < len ? alphabet[(v >> 6) & 63] : '=';
        out[j++] = i + 2 < len ? alphabet[v & 63] : '=';
    }
    out[j] = '\0';
    return out;
}

static char *jsonEscape(const char *src) {
    size_t len = strlen(src);
    if (len > (SIZE_MAX - 1) / 6) return NULL;
    char *out = malloc(len * 6 + 1);
    if (out == NULL) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') {
            out[j++] = '\\';
            out[j++] = c;
        } else if (c < 0x20) {
            snprintf(out + j, 7, "\\u%04x", c);
            j += 6;
        } else {
            out[j++] = c;
        }
    }
    out[j] = '\0';
    return out;
}

static rsRetVal putRecord(wrkrInstanceData_t *pWrkrData, const char *message, const char *partitionKey) {
    instanceData *const pData = pWrkrData->pData;
    char *encoded = NULL, *stream = NULL, *key = NULL, *body = NULL;
    struct curl_slist *headers = NULL, *next;
    long status = 0;
    responseBuf_t response = {{0}, 0};
    CURLcode curlRet;
    DEFiRet;

    size_t msgLen = strlen(message);
    size_t keyLen = partitionKey == NULL ? 0 : strlen(partitionKey);
    if (keyLen == 0 || keyLen > 256) {
        LogError(0, RS_RET_PARAM_ERROR, "omkinesis: partition key must be 1..256 bytes");
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }
    /* Kinesis counts the partition key and raw data toward the configured limit. */
    if (msgLen > (size_t)pData->maxRecordSize - keyLen) {
        LogError(0, RS_RET_PARAM_ERROR, "omkinesis: record exceeds configured max_record_size");
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }
    CHKmalloc(encoded = base64Encode((const unsigned char *)message, msgLen));
    CHKmalloc(stream = jsonEscape((const char *)pData->stream));
    CHKmalloc(key = jsonEscape(partitionKey));
    size_t bodyLen = strlen(encoded) + strlen(stream) + strlen(key) + 64;
    CHKmalloc(body = malloc(bodyLen));
    int n = snprintf(body, bodyLen, "{\"StreamName\":\"%s\",\"PartitionKey\":\"%s\",\"Data\":\"%s\"}", stream, key,
                     encoded);
    if (n < 0 || (size_t)n >= bodyLen) ABORT_FINALIZE(RS_RET_ERR);

    next = curl_slist_append(headers, "Content-Type: application/x-amz-json-1.1");
    CHKmalloc(next);
    headers = next;
    next = curl_slist_append(headers, "X-Amz-Target: Kinesis_20131202.PutRecord");
    CHKmalloc(next);
    headers = next;
    if (pData->sessionToken != NULL) {
        size_t headerLen = strlen((const char *)pData->sessionToken) + 24;
        char *tokenHeader;
        CHKmalloc(tokenHeader = malloc(headerLen));
        snprintf(tokenHeader, headerLen, "X-Amz-Security-Token: %s", pData->sessionToken);
        next = curl_slist_append(headers, tokenHeader);
        free(tokenHeader);
        CHKmalloc(next);
        headers = next;
    }

    curl_easy_setopt(pWrkrData->curl, CURLOPT_URL, pData->url);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_AWS_SIGV4, pData->sigv4);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_USERPWD, pData->userpwd);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_POSTFIELDSIZE, (long)n);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_TIMEOUT, pData->timeout);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_WRITEFUNCTION, captureResponse);
    curl_easy_setopt(pWrkrData->curl, CURLOPT_WRITEDATA, &response);
    curlRet = curl_easy_perform(pWrkrData->curl);
    if (curlRet != CURLE_OK) {
        LogError(0, RS_RET_SUSPENDED, "omkinesis: PutRecord failed: %s", curl_easy_strerror(curlRet));
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }
    curl_easy_getinfo(pWrkrData->curl, CURLINFO_RESPONSE_CODE, &status);
    if (status != 200) {
        const int retryable =
            status == 429 || status >= 500 ||
            (status == 400 && (strstr(response.data, "ProvisionedThroughputExceededException") != NULL ||
                               strstr(response.data, "InternalFailure") != NULL ||
                               strstr(response.data, "KMSThrottlingException") != NULL));
        iRet = retryable ? RS_RET_SUSPENDED : RS_RET_ERR;
        LogError(0, iRet, "omkinesis: PutRecord returned HTTP %ld%s", status, retryable ? " (retryable)" : "");
        FINALIZE;
    }
finalize_it:
    curl_slist_free_all(headers);
    free(encoded);
    free(stream);
    free(key);
    free(body);
    RETiRet;
}

BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
ENDbeginCnfLoad
BEGINendCnfLoad
    CODESTARTendCnfLoad;
ENDendCnfLoad
BEGINcheckCnf
    CODESTARTcheckCnf;
ENDcheckCnf
BEGINactivateCnf
    CODESTARTactivateCnf;
    runModConf = pModConf;
ENDactivateCnf
BEGINfreeCnf
    CODESTARTfreeCnf;
ENDfreeCnf
BEGINcreateInstance
    CODESTARTcreateInstance;
ENDcreateInstance
BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
    pWrkrData->curl = curl_easy_init();
    if (pWrkrData->curl == NULL) ABORT_FINALIZE(RS_RET_OBJ_CREATION_FAILED);
finalize_it:
ENDcreateWrkrInstance
BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
    if (pWrkrData->curl != NULL) curl_easy_cleanup(pWrkrData->curl);
ENDfreeWrkrInstance
BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURERepeatedMsgReduction) iRet = RS_RET_OK;
ENDisCompatibleWithFeature
BEGINfreeInstance
    CODESTARTfreeInstance;
    free(pData->stream);
    free(pData->region);
    free(pData->partitionKey);
    free(pData->partitionKeyTplName);
    free(pData->tplName);
    free(pData->accessKey);
    free(pData->secretKey);
    free(pData->sessionToken);
    free(pData->endpoint);
    free(pData->url);
    free(pData->sigv4);
    free(pData->userpwd);
ENDfreeInstance
BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    dbgprintf("omkinesis stream='%s' region='%s'\n", pData->stream, pData->region);
ENDdbgPrintInstInfo
BEGINtryResume
    CODESTARTtryResume;
ENDtryResume
BEGINbeginTransaction
    CODESTARTbeginTransaction;
ENDbeginTransaction
BEGINdoAction
    CODESTARTdoAction;
    CHKiRet(putRecord(pWrkrData, (const char *)ppString[0],
                      pWrkrData->pData->partitionKeyTplName == NULL ? (const char *)pWrkrData->pData->partitionKey
                                                                    : (const char *)ppString[1]));
finalize_it:
ENDdoAction
BEGINendTransaction
    CODESTARTendTransaction;
ENDendTransaction

BEGINnewActInst
    struct cnfparamvals *pvals = NULL;
    int i, nTpls;
    uchar *tplToUse = NULL;
    const char *env;
    CODESTARTnewActInst;
    pvals = nvlstGetParams(lst, &actpblk, NULL);
    if (pvals == NULL) ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    CHKiRet(createInstance(&pData));
    pData->timeout = 30;
    pData->maxRecordSize = 1024 * 1024;
    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        const char *name = actpblk.descr[i].name;
        if (!strcmp(name, "timeout")) {
            pData->timeout = pvals[i].val.d.n;
        } else if (!strcmp(name, "max_record_size")) {
            pData->maxRecordSize = pvals[i].val.d.n;
        } else {
            uchar **target = NULL;
            if (!strcmp(name, "stream"))
                target = &pData->stream;
            else if (!strcmp(name, "region"))
                target = &pData->region;
            else if (!strcmp(name, "partition_key"))
                target = &pData->partitionKey;
            else if (!strcmp(name, "partition_key_template"))
                target = &pData->partitionKeyTplName;
            else if (!strcmp(name, "template"))
                target = &pData->tplName;
            else if (!strcmp(name, "access_key"))
                target = &pData->accessKey;
            else if (!strcmp(name, "secret_key"))
                target = &pData->secretKey;
            else if (!strcmp(name, "session_token"))
                target = &pData->sessionToken;
            else if (!strcmp(name, "endpoint"))
                target = &pData->endpoint;
            if (target == NULL) ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
            CHKmalloc(*target = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        }
    }
    if (pData->accessKey == NULL && (env = getenv("AWS_ACCESS_KEY_ID")) != NULL)
        CHKmalloc(pData->accessKey = (uchar *)strdup(env));
    if (pData->secretKey == NULL && (env = getenv("AWS_SECRET_ACCESS_KEY")) != NULL)
        CHKmalloc(pData->secretKey = (uchar *)strdup(env));
    if (pData->sessionToken == NULL && (env = getenv("AWS_SESSION_TOKEN")) != NULL)
        CHKmalloc(pData->sessionToken = (uchar *)strdup(env));
    if (pData->region == NULL && (env = getenv("AWS_REGION")) != NULL) CHKmalloc(pData->region = (uchar *)strdup(env));
    if (pData->stream == NULL || *pData->stream == '\0' ||
        ((pData->partitionKey == NULL) == (pData->partitionKeyTplName == NULL)) ||
        (pData->partitionKey != NULL &&
         (*pData->partitionKey == '\0' || strlen((const char *)pData->partitionKey) > 256)) ||
        (pData->partitionKeyTplName != NULL && *pData->partitionKeyTplName == '\0') || pData->region == NULL ||
        !validRegion((const char *)pData->region) || pData->accessKey == NULL || *pData->accessKey == '\0' ||
        pData->secretKey == NULL || *pData->secretKey == '\0' || pData->timeout < 1 || pData->timeout > 300 ||
        pData->maxRecordSize < 1024 * 1024 || pData->maxRecordSize > 10 * 1024 * 1024 ||
        (pData->endpoint != NULL && !validEndpoint((const char *)pData->endpoint))) {
        LogError(0, RS_RET_PARAM_ERROR,
                 "omkinesis: invalid stream, partition_key or partition_key_template, region, credentials, endpoint, "
                 "or timeout");
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }
    size_t len = strlen((const char *)pData->region) + 40;
    CHKmalloc(pData->sigv4 = malloc(len));
    snprintf(pData->sigv4, len, "aws:amz:%s:kinesis", pData->region);
    len = strlen((const char *)pData->accessKey) + strlen((const char *)pData->secretKey) + 2;
    CHKmalloc(pData->userpwd = malloc(len));
    snprintf(pData->userpwd, len, "%s:%s", pData->accessKey, pData->secretKey);
    if (pData->endpoint != NULL) {
        CHKmalloc(pData->url = strdup((const char *)pData->endpoint));
    } else {
        len = strlen((const char *)pData->region) + 40;
        CHKmalloc(pData->url = malloc(len));
        snprintf(pData->url, len, "https://kinesis.%s.amazonaws.com/", pData->region);
    }
    nTpls = pData->partitionKeyTplName == NULL ? 1 : 2;
    CODE_STD_STRING_REQUESTnewActInst(nTpls);
    CHKmalloc(tplToUse =
                  (uchar *)strdup(pData->tplName == NULL ? "RSYSLOG_TraditionalFileFormat" : (char *)pData->tplName));
    CHKiRet(OMSRsetEntry(*ppOMSR, 0, tplToUse, OMSR_NO_RQD_TPL_OPTS));
    if (nTpls == 2) {
        CHKmalloc(tplToUse = (uchar *)strdup((const char *)pData->partitionKeyTplName));
        CHKiRet(OMSRsetEntry(*ppOMSR, 1, tplToUse, OMSR_NO_RQD_TPL_OPTS));
    }
    CODE_STD_FINALIZERnewActInst;
    cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

BEGINmodExit
    CODESTARTmodExit;
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
    if (curl_global_init(CURL_GLOBAL_ALL) != 0) ABORT_FINALIZE(RS_RET_OBJ_CREATION_FAILED);
ENDmodInit
