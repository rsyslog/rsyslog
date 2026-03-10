/* omazuredce.c
 * Prototype output module for Azure Monitor Logs Ingestion API (DCE/DCR).
 *
 * This module batches messages into JSON under 1 MiB and posts each batch to
 * the Azure Monitor Logs Ingestion API endpoint.
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
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <json.h>
#include <curl/curl.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("omazuredce")

/* The maximum size of an API call to the Azure Log Ingestion API is 1MB, which (I assume) means that the call itself
 * with the headers and so on must not exceed this size */
#define AZURE_MAX_BATCH_BYTES (1024 * 1024)

/* The max size of field values is 64KB, when that limit is reached the rest is turnecated.
    https://learn.microsoft.com/en-us/azure/azure-monitor/fundamentals/service-limits#logs-ingestion-api */
#define AZURE_MAX_FIELD_BYTES (64 * 1024)
#define AZURE_OAUTH_SCOPE "https://monitor.azure.com/.default"
#define AZURE_AUTH_TOKEN_FALLBACK_LEN 4096
#define AZURE_HTTP_EXTRA_OVERHEAD 512

DEF_OMOD_STATIC_DATA;

typedef struct _instanceData {
    uchar *templateName;
    uchar *clientID;
    uchar *clientSecret;
    uchar *tenantID;
    uchar *dceURL;
    uchar *dcrID;
    uchar *tableName;
    uchar *accessToken;
    pthread_mutex_t accessTokenLock;
    sbool accessTokenLockInit;
    int maxBatchBytes;
    int flushTimeoutMs;
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData;
    char *batchBuf; /* active JSON array, always starts with '[' */
    size_t batchLen;
    size_t recordCount;
    uint64_t lastMessageTimeMs;
    pthread_mutex_t batchLock;
    pthread_cond_t timerCond;
    pthread_t timerThread;
    sbool timerThreadRunning;
    sbool stopTimerThread;
    rsRetVal pendingAsyncFlushRet;
} wrkrInstanceData_t;

static struct cnfparamdescr actpdescr[] = {
    {"template", eCmdHdlrGetWord, 0},  {"client_id", eCmdHdlrString, 0},    {"client_secret", eCmdHdlrString, 0},
    {"tenant_id", eCmdHdlrString, 0},  {"dce_url", eCmdHdlrString, 0},      {"dcr_id", eCmdHdlrString, 0},
    {"table_name", eCmdHdlrString, 0}, {"max_batch_bytes", eCmdHdlrInt, 0}, {"flush_timeout_ms", eCmdHdlrNonNegInt, 0}};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

struct modConfData_s {
    rsconf_t *pConf;
};
static modConfData_t *runModConf = NULL;

static inline const char *safeStr(const uchar *s) {
    return (s == NULL) ? "<unset>" : (const char *)s;
}

typedef struct tokenRespBuf_s {
    char *data;
    size_t len;
} tokenRespBuf_t;

static size_t tokenWriteCb(void *contents, size_t size, size_t nmemb, void *userp) {
    const size_t realsz = size * nmemb;
    tokenRespBuf_t *buf = (tokenRespBuf_t *)userp;
    char *newData = realloc(buf->data, buf->len + realsz + 1);
    if (newData == NULL) {
        return 0;
    }
    buf->data = newData;
    memcpy(buf->data + buf->len, contents, realsz);
    buf->len += realsz;
    buf->data[buf->len] = '\0';
    return realsz;
}

static rsRetVal copyAccessToken(instanceData *pData, char **tokenCopy) {
    int lockHeld = 0;
    DEFiRet;

    *tokenCopy = NULL;
    if (pthread_mutex_lock(&pData->accessTokenLock) != 0) {
        ABORT_FINALIZE(RS_RET_SYS_ERR);
    }
    lockHeld = 1;
    if (pData->accessToken != NULL && pData->accessToken[0] != '\0') {
        *tokenCopy = strdup((const char *)pData->accessToken);
        if (*tokenCopy == NULL) {
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
    }
    if (pthread_mutex_unlock(&pData->accessTokenLock) != 0) {
        lockHeld = 0;
        ABORT_FINALIZE(RS_RET_SYS_ERR);
    }
    lockHeld = 0;

finalize_it:
    if (lockHeld) {
        (void)pthread_mutex_unlock(&pData->accessTokenLock);
    }
    RETiRet;
}

static size_t getAccessTokenLen(instanceData *pData) {
    size_t tokenLen;

    if (pthread_mutex_lock(&pData->accessTokenLock) != 0) {
        return AZURE_AUTH_TOKEN_FALLBACK_LEN;
    }
    tokenLen = (pData->accessToken == NULL || pData->accessToken[0] == '\0') ? AZURE_AUTH_TOKEN_FALLBACK_LEN
                                                                             : strlen((const char *)pData->accessToken);
    (void)pthread_mutex_unlock(&pData->accessTokenLock);
    return tokenLen;
}

static rsRetVal requestAccessToken(instanceData *pData) {
    char tokenURL[512];
    char *body = NULL;
    char *escClientID = NULL;
    char *escClientSecret = NULL;
    char *escScope = NULL;
    CURL *curl = NULL;
    CURLcode curlRes;
    long httpCode = 0;
    size_t bodyLen;
    struct curl_slist *headers = NULL;
    tokenRespBuf_t response = {NULL, 0};
    char *token = NULL;
    struct json_object *tokenResp = NULL;
    struct json_object *tokenField = NULL;
    struct json_tokener *tok = NULL;
    const char *tokenStr = NULL;
    enum fjson_tokener_error tokErr;
    size_t responseLen;
    size_t parseEnd;
    int n;
    DEFiRet;

    if (pData->clientID == NULL || pData->clientSecret == NULL || pData->tenantID == NULL) {
        LogError(0, RS_RET_PARAM_ERROR,
                 "omazuredce: cannot request access token, missing one of client_id/client_secret/tenant_id");
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    n = snprintf(tokenURL, sizeof(tokenURL), "https://login.microsoftonline.com/%s/oauth2/v2.0/token",
                 (char *)pData->tenantID);
    if (n < 0 || (size_t)n >= sizeof(tokenURL)) {
        LogError(0, RS_RET_ERR, "omazuredce: tenant_id is too long to build OAuth token URL");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    curl = curl_easy_init();
    if (curl == NULL) {
        LogError(0, RS_RET_ERR, "omazuredce: curl_easy_init failed while requesting access token");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    escClientID = curl_easy_escape(curl, (char *)pData->clientID, 0);
    escClientSecret = curl_easy_escape(curl, (char *)pData->clientSecret, 0);
    escScope = curl_easy_escape(curl, AZURE_OAUTH_SCOPE, 0);
    if (escClientID == NULL || escClientSecret == NULL || escScope == NULL) {
        LogError(0, RS_RET_ERR, "omazuredce: failed escaping OAuth form values");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    bodyLen = strlen("client_id=&scope=&client_secret=&grant_type=client_credentials") + strlen(escClientID) +
              strlen(escScope) + strlen(escClientSecret) + 1;
    body = malloc(bodyLen);
    if (body == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

    n = snprintf(body, bodyLen, "client_id=%s&scope=%s&client_secret=%s&grant_type=client_credentials", escClientID,
                 escScope, escClientSecret);
    if (n < 0 || (size_t)n >= bodyLen) {
        LogError(0, RS_RET_ERR, "omazuredce: failed building OAuth request body");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_URL, tokenURL);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, tokenWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    DBGPRINTF("omazuredce: requesting OAuth token for tenant_id='%s' client_id='%s'\n", safeStr(pData->tenantID),
              safeStr(pData->clientID));
    curlRes = curl_easy_perform(curl);
    if (curlRes != CURLE_OK) {
        LogError(0, RS_RET_IO_ERROR, "omazuredce: token request failed: %s", curl_easy_strerror(curlRes));
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if (httpCode < 200 || httpCode >= 300) {
        LogError(0, RS_RET_IO_ERROR, "omazuredce: token request HTTP status=%ld response='%s'", httpCode,
                 response.data == NULL ? "" : response.data);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }

    responseLen = response.data == NULL ? 0 : response.len;
    if ((tok = json_tokener_new()) == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    if (responseLen > 2147483647) { /* INT_MAX */
        LogError(0, RS_RET_IO_ERROR, "omazuredce: token response is too large for JSON parser: %zu", responseLen);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    tokenResp = json_tokener_parse_ex(tok, response.data == NULL ? "" : response.data, (int)responseLen);
    tokErr = fjson_tokener_get_error(tok);

    parseEnd = 0;
    if (tok != NULL) {
        parseEnd = (size_t)tok->char_offset;

        while (parseEnd < responseLen && response.data != NULL && isspace((unsigned char)response.data[parseEnd])) {
            ++parseEnd;
        }
        if (tokErr != fjson_tokener_success || parseEnd != responseLen || tokenResp == NULL ||
            !json_object_object_get_ex(tokenResp, "access_token", &tokenField) ||
            !json_object_is_type(tokenField, json_type_string)) {
            LogError(0, RS_RET_IO_ERROR, "omazuredce: access_token not found in token response");
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }
        tokenStr = json_object_get_string(tokenField);
        if (tokenStr == NULL || tokenStr[0] == '\0') {
            LogError(0, RS_RET_IO_ERROR, "omazuredce: access_token is empty in token response");
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }
        CHKmalloc(token = strdup(tokenStr));

        if (pthread_mutex_lock(&pData->accessTokenLock) != 0) {
            ABORT_FINALIZE(RS_RET_SYS_ERR);
        }
        free(pData->accessToken);
        pData->accessToken = (uchar *)token;
        if (pthread_mutex_unlock(&pData->accessTokenLock) != 0) {
            ABORT_FINALIZE(RS_RET_SYS_ERR);
        }
        token = NULL;
        DBGPRINTF("omazuredce: access token acquired successfully (len=%zu)\n", strlen(tokenStr));
    }

    finalize_it:
        if (token != NULL) free(token);
        free(response.data);
        free(body);
        if (escClientID != NULL) curl_free(escClientID);
        if (escClientSecret != NULL) curl_free(escClientSecret);
        if (escScope != NULL) curl_free(escScope);
        if (headers != NULL) curl_slist_free_all(headers);
        if (curl != NULL) curl_easy_cleanup(curl);
	    if (tok != NULL) json_tokener_free(tok);
	    if (tokenResp != NULL) json_object_put(tokenResp);
	    RETiRet;
}

	static uint64_t nowMs(void) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return ((uint64_t)tv.tv_sec * 1000ULL) + ((uint64_t)tv.tv_usec / 1000ULL);
    }

    static size_t decimalDigits(size_t v) {
        size_t n = 1;
        while (v >= 10) {
            v /= 10;
            ++n;
        }
        return n;
    }

    /* Estimate total HTTP request bytes (headers + body) for max_batch_bytes enforcement. */
    static size_t estimateHttpRequestBytes(instanceData * pData, size_t payloadLen) {
        const char *dce = (const char *)pData->dceURL;
        size_t dceLen = (dce == NULL) ? 0 : strlen(dce);
        size_t slashLen = 0;
        size_t dcrLen = (pData->dcrID == NULL) ? 0 : strlen((const char *)pData->dcrID);
        size_t tableLen = (pData->tableName == NULL) ? 0 : strlen((const char *)pData->tableName);
        size_t tokenLen = getAccessTokenLen(pData);
        size_t urlLen;
        size_t total;

        if (dceLen > 0 && dce[dceLen - 1] != '/') {
            slashLen = 1;
        }

        urlLen = dceLen + slashLen + strlen("dataCollectionRules/") + dcrLen + strlen("/streams/") + tableLen +
                 strlen("?api-version=2023-01-01");

        total = payloadLen;
        total += strlen("POST ") + urlLen + strlen(" HTTP/1.1\r\n");
        total += strlen("Host: ") + urlLen + strlen("\r\n");
        total += strlen("Content-Type: application/json\r\n");
        total += strlen("Authorization: Bearer ") + tokenLen + strlen("\r\n");
        total += strlen("Content-Length: ") + decimalDigits(payloadLen) + strlen("\r\n");
        total += strlen("\r\n");
        total += AZURE_HTTP_EXTRA_OVERHEAD;

        return total;
    }

    static rsRetVal postBatchToAzure(instanceData * pData, wrkrInstanceData_t * pWrkrData, size_t payloadLen) {
        const char *slash;
        char *url = NULL;
        char *authHeader = NULL;
        char *dce = NULL;
        size_t urlLen;
        CURL *curl = NULL;
        CURLcode curlRes;
        long httpCode = 0;
        tokenRespBuf_t response = {NULL, 0};
        struct curl_slist *headers = NULL;
        char *accessToken = NULL;
        int n;
        DEFiRet;

        if (pData->dceURL == NULL || pData->dcrID == NULL || pData->tableName == NULL) {
            LogError(0, RS_RET_PARAM_ERROR, "omazuredce: cannot post batch, missing one of dce_url/dcr_id/table_name");
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }

        CHKiRet(copyAccessToken(pData, &accessToken));
        if (accessToken == NULL) {
            CHKiRet(requestAccessToken(pData));
            CHKiRet(copyAccessToken(pData, &accessToken));
            if (accessToken == NULL) {
                ABORT_FINALIZE(RS_RET_ERR);
            }
        }

        dce = (char *)pData->dceURL;
        if (dce[0] != '\0' && dce[strlen(dce) - 1] == '/') {
            slash = "";
        } else {
            slash = "/";
        }
        urlLen = strlen(dce) + strlen(slash) + strlen("/dataCollectionRules/") + strlen((char *)pData->dcrID) +
                 strlen("/streams/") + strlen((char *)pData->tableName) + strlen("?api-version=2023-01-01") + 1;
        CHKmalloc(url = malloc(urlLen));
        n = snprintf(url, urlLen, "%s%sdataCollectionRules/%s/streams/%s?api-version=2023-01-01", dce, slash,
                     (char *)pData->dcrID, (char *)pData->tableName);
        if (n < 0 || (size_t)n >= urlLen) {
            LogError(0, RS_RET_ERR, "omazuredce: failed building Azure DCE request URL");
            ABORT_FINALIZE(RS_RET_ERR);
        }

        {
            const size_t tokenLen = strlen(accessToken);
            const size_t requestLen = estimateHttpRequestBytes(pData, payloadLen);
            if (requestLen > (size_t)pData->maxBatchBytes) {
                LogError(0, RS_RET_ERR,
                         "omazuredce: batch exceeds max request size before send, payload_bytes=%zu "
                         "estimated_request_bytes=%zu max_batch_bytes=%d",
                         payloadLen, requestLen, pData->maxBatchBytes);
                ABORT_FINALIZE(RS_RET_ERR);
            }

            curl = curl_easy_init();
            if (curl == NULL) {
                LogError(0, RS_RET_ERR, "omazuredce: curl_easy_init failed while posting batch");
                ABORT_FINALIZE(RS_RET_ERR);
            }

            const size_t authHeaderLen = strlen("Authorization: Bearer ") + tokenLen + 1;
            CHKmalloc(authHeader = malloc(authHeaderLen));
            n = snprintf(authHeader, authHeaderLen, "Authorization: Bearer %s", accessToken);
            if (n < 0 || (size_t)n >= authHeaderLen) {
                LogError(0, RS_RET_ERR, "omazuredce: failed building Authorization header");
                ABORT_FINALIZE(RS_RET_ERR);
            }

            headers = curl_slist_append(headers, "Content-Type: application/json");
            headers = curl_slist_append(headers, authHeader);
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, pWrkrData->batchBuf);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payloadLen);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, tokenWriteCb);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);

            DBGPRINTF("omazuredce: posting batch url='%s' bytes=%zu\n", url, payloadLen);
            curlRes = curl_easy_perform(curl);
            if (curlRes != CURLE_OK) {
                LogError(0, RS_RET_SUSPENDED, "omazuredce: batch post failed: %s", curl_easy_strerror(curlRes));
                ABORT_FINALIZE(RS_RET_SUSPENDED);
            }
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
            if (httpCode >= 200 && httpCode < 300) {
                DBGPRINTF("omazuredce: batch post successful status=%ld response='%s'\n", httpCode,
                          response.data == NULL ? "" : response.data);
                LogMsg(0, RS_RET_OK, LOG_INFO, "omazuredce: posted batch records=%zu stream=%s", pWrkrData->recordCount,
                       safeStr(pData->tableName));
                FINALIZE;
            }
            if (httpCode == 401) {
                DBGPRINTF("omazuredce: batch post got 401, refreshing access token before action-engine retry\n");
                CHKiRet(requestAccessToken(pData));
                LogError(0, RS_RET_SUSPENDED,
                         "omazuredce: batch post HTTP status=%ld response='%s' (token refreshed, will retry via action "
                         "engine)",
                         httpCode, response.data == NULL ? "" : response.data);
                ABORT_FINALIZE(RS_RET_SUSPENDED);
            }
            if (httpCode == 408 || httpCode == 429 || httpCode >= 500) {
                LogError(0, RS_RET_SUSPENDED, "omazuredce: batch post HTTP status=%ld response='%s' (will retry)",
                         httpCode, response.data == NULL ? "" : response.data);
                ABORT_FINALIZE(RS_RET_SUSPENDED);
            }
            LogError(0, RS_RET_IO_ERROR, "omazuredce: batch post HTTP status=%ld response='%s'", httpCode,
                     response.data == NULL ? "" : response.data);
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }

    finalize_it:
        free(url);
        free(authHeader);
        free(accessToken);
        free(response.data);
        if (headers != NULL) curl_slist_free_all(headers);
        if (curl != NULL) curl_easy_cleanup(curl);
        RETiRet;
    }

    static rsRetVal appendChar(wrkrInstanceData_t * pWrkrData, const char c) {
        DEFiRet;
        if (pWrkrData->batchLen + 1 > (size_t)pWrkrData->pData->maxBatchBytes) {
            ABORT_FINALIZE(RS_RET_ERR);
        }
        pWrkrData->batchBuf[pWrkrData->batchLen++] = c;
    finalize_it:
        RETiRet;
    }

    static rsRetVal appendRaw(wrkrInstanceData_t * pWrkrData, const char *s, const size_t len) {
        DEFiRet;
        if (pWrkrData->batchLen + len > (size_t)pWrkrData->pData->maxBatchBytes) {
            ABORT_FINALIZE(RS_RET_ERR);
        }
        memcpy(pWrkrData->batchBuf + pWrkrData->batchLen, s, len);
        pWrkrData->batchLen += len;
    finalize_it:
        RETiRet;
    }

    static rsRetVal buildRecordJson(const char *msg, char **recordJson, size_t *recordLen) {
        struct json_object *parsedObj = NULL;
        struct json_tokener *tok = NULL;
        const char *jsonText;
        enum fjson_tokener_error tokErr;
        size_t parseEnd;
        size_t msgLen;
        DEFiRet;

        *recordJson = NULL;
        *recordLen = 0;
        if (msg == NULL) msg = "";
        msgLen = strlen(msg);

        if ((tok = json_tokener_new()) == NULL) {
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }

        if (msgLen > 2147483647) { /* INT_MAX */
            LogError(0, RS_RET_OVERSIZE_MSG, "omazuredce: message is too large for JSON parser: %zu", msgLen);
            ABORT_FINALIZE(RS_RET_OVERSIZE_MSG);
        }

        parsedObj = json_tokener_parse_ex(tok, msg, (int)msgLen);
        tokErr = fjson_tokener_get_error(tok);
        parseEnd = (size_t)tok->char_offset;
        while (parseEnd < msgLen && isspace((unsigned char)msg[parseEnd])) {
            ++parseEnd;
        }
        if (tokErr != fjson_tokener_success || parsedObj == NULL || !json_object_is_type(parsedObj, json_type_object) ||
            parseEnd != msgLen) {
            LogError(0, RS_RET_ERR, "omazuredce: message template must render a JSON object, got: '%s'", msg);
            ABORT_FINALIZE(RS_RET_ERR);
        }

        jsonText = json_object_to_json_string_ext(parsedObj, JSON_C_TO_STRING_PLAIN);
        if (jsonText == NULL) {
            ABORT_FINALIZE(RS_RET_ERR);
        }
        CHKmalloc(*recordJson = strdup(jsonText));
        *recordLen = strlen(*recordJson);

    finalize_it:
        if (tok != NULL) json_tokener_free(tok);
        if (parsedObj != NULL) json_object_put(parsedObj);
        if (iRet != RS_RET_OK) {
            free(*recordJson);
            *recordJson = NULL;
            *recordLen = 0;
        }
        RETiRet;
    }

    static void resetBatch(wrkrInstanceData_t * pWrkrData) {
        pWrkrData->batchLen = 0;
        pWrkrData->recordCount = 0;
        pWrkrData->batchBuf[pWrkrData->batchLen++] = '[';
        DBGPRINTF("omazuredce[%p]: reset batch buffer\n", pWrkrData);
    }

    static rsRetVal flushBatchUnlocked(wrkrInstanceData_t * pWrkrData) {
        instanceData *const pData = pWrkrData->pData;
        size_t openBatchLen = 0;
        size_t payloadLen = 0;
        size_t requestLen;
        DEFiRet;
        DBGPRINTF("omazuredce[%p]: flushBatch enter, records=%zu currentLen=%zu\n", pWrkrData, pWrkrData->recordCount,
                  pWrkrData->batchLen);

        if (pWrkrData->recordCount == 0) {
            FINALIZE;
        }

        openBatchLen = pWrkrData->batchLen;
        CHKiRet(appendChar(pWrkrData, ']'));
        payloadLen = pWrkrData->batchLen;
        pWrkrData->batchBuf[payloadLen] = '\0';
        requestLen = estimateHttpRequestBytes(pData, payloadLen);
        if (requestLen > (size_t)pData->maxBatchBytes) {
            LogError(0, RS_RET_ERR,
                     "omazuredce: dropping over-sized batch, payload_bytes=%zu estimated_request_bytes=%zu "
                     "max_batch_bytes=%d",
                     payloadLen, requestLen, pData->maxBatchBytes);
            resetBatch(pWrkrData);
            FINALIZE;
        }
        DBGPRINTF("omazuredce[%p]: flush batch posting records=%zu payloadBytes=%zu payload='%s'\n", pWrkrData,
                  pWrkrData->recordCount, payloadLen, pWrkrData->batchBuf);
        CHKiRet(postBatchToAzure(pData, pWrkrData, payloadLen));
        DBGPRINTF("omazuredce[%p]: flushed batch records=%zu payloadBytes=%zu\n", pWrkrData, pWrkrData->recordCount,
                  payloadLen);

        resetBatch(pWrkrData);

    finalize_it:
        if (iRet != RS_RET_OK && pWrkrData->recordCount > 0 && pWrkrData->batchLen == payloadLen &&
            payloadLen > openBatchLen && pWrkrData->batchBuf[payloadLen - 1] == ']') {
            /* Restore the open JSON array so a retry keeps appending valid payload. */
            pWrkrData->batchLen = openBatchLen;
            pWrkrData->batchBuf[pWrkrData->batchLen] = '\0';
        }
        RETiRet;
    }

    static rsRetVal flushBatch(wrkrInstanceData_t * pWrkrData) {
        DEFiRet;
        int lockHeld = 0;
        if (pthread_mutex_lock(&pWrkrData->batchLock) != 0) {
            ABORT_FINALIZE(RS_RET_SYS_ERR);
        }
        lockHeld = 1;
        CHKiRet(flushBatchUnlocked(pWrkrData));
        if (pthread_mutex_unlock(&pWrkrData->batchLock) != 0) {
            lockHeld = 0;
            ABORT_FINALIZE(RS_RET_SYS_ERR);
        }
        lockHeld = 0;
    finalize_it:
        if (lockHeld) {
            (void)pthread_mutex_unlock(&pWrkrData->batchLock);
        }
        RETiRet;
    }

    static rsRetVal addMessageToBatchUnlocked(wrkrInstanceData_t * pWrkrData, const char *msg) {
        char *recordJson = NULL;
        size_t recordLen = 0;
        const size_t maxBatchBytes = (size_t)pWrkrData->pData->maxBatchBytes;
        size_t recLen;
        size_t projectedLen;
        size_t projectedRequestLen;
        DEFiRet;
        CHKiRet(buildRecordJson(msg, &recordJson, &recordLen));
        recLen = (pWrkrData->recordCount > 0 ? 1 : 0) + recordLen;
        DBGPRINTF("omazuredce[%p]: add message recordLen=%zu currentBatchLen=%zu\n", pWrkrData, recLen,
                  pWrkrData->batchLen);

        /* +1 reserves room for the trailing ']' when the batch is flushed */
        projectedLen = pWrkrData->batchLen + recLen + 1;
        projectedRequestLen = estimateHttpRequestBytes(pWrkrData->pData, projectedLen);
        if ((projectedLen > maxBatchBytes || projectedRequestLen > maxBatchBytes) && pWrkrData->recordCount > 0) {
            DBGPRINTF("omazuredce[%p]: batch limit reached, forcing flush before append\n", pWrkrData);
            CHKiRet(flushBatchUnlocked(pWrkrData));
            projectedLen = pWrkrData->batchLen + recLen + 1;
            projectedRequestLen = estimateHttpRequestBytes(pWrkrData->pData, projectedLen);
        }

        if (projectedLen > maxBatchBytes || projectedRequestLen > maxBatchBytes) {
            LogError(0, RS_RET_ERR,
                     "omazuredce: dropping over-sized log record, record_len=%zu projected_payload_bytes=%zu "
                     "projected_request_bytes=%zu max_batch_bytes=%d",
                     recordLen, projectedLen, projectedRequestLen, pWrkrData->pData->maxBatchBytes);
            ABORT_FINALIZE(RS_RET_OK);
        }

        if (pWrkrData->recordCount > 0) CHKiRet(appendChar(pWrkrData, ','));
        CHKiRet(appendRaw(pWrkrData, recordJson, recordLen));
        pWrkrData->recordCount++;
        DBGPRINTF("omazuredce[%p]: message appended, recordCount=%zu batchLen=%zu\n", pWrkrData, pWrkrData->recordCount,
                  pWrkrData->batchLen);

    finalize_it:
        free(recordJson);
        RETiRet;
    }

    static rsRetVal retryPendingAsyncFlushUnlocked(wrkrInstanceData_t * pWrkrData) {
        DEFiRet;

        if (pWrkrData->pendingAsyncFlushRet == RS_RET_OK) {
            FINALIZE;
        }

        DBGPRINTF("omazuredce[%p]: retrying pending async flush ret=%d records=%zu\n", pWrkrData,
                  pWrkrData->pendingAsyncFlushRet, pWrkrData->recordCount);
        CHKiRet(flushBatchUnlocked(pWrkrData));
        pWrkrData->pendingAsyncFlushRet = RS_RET_OK;

    finalize_it:
        RETiRet;
    }

    static void *batchTimerThread(void *arg) {
        wrkrInstanceData_t *const pWrkrData = (wrkrInstanceData_t *)arg;
        instanceData *const pData = pWrkrData->pData;
        DBGPRINTF("omazuredce[%p]: timer thread started with flush_timeout_ms=%d\n", pWrkrData, pData->flushTimeoutMs);

        if (pthread_mutex_lock(&pWrkrData->batchLock) != 0) {
            DBGPRINTF("omazuredce[%p]: timer thread failed to acquire batch lock\n", pWrkrData);
            return NULL;
        }

        while (!pWrkrData->stopTimerThread) {
            if (pWrkrData->pendingAsyncFlushRet != RS_RET_OK) {
                pthread_cond_wait(&pWrkrData->timerCond, &pWrkrData->batchLock);
                continue;
            }
            if (pData->flushTimeoutMs <= 0 || pWrkrData->recordCount == 0) {
                pthread_cond_wait(&pWrkrData->timerCond, &pWrkrData->batchLock);
                continue;
            }

            {
                const uint64_t now = nowMs();
                const uint64_t elapsed =
                    (now >= pWrkrData->lastMessageTimeMs) ? (now - pWrkrData->lastMessageTimeMs) : 0;

                if (elapsed >= (uint64_t)pData->flushTimeoutMs) {
                    rsRetVal flushRet;
                    DBGPRINTF("omazuredce[%p]: timer flush triggered elapsed=%llums records=%zu\n", pWrkrData,
                              (unsigned long long)elapsed, pWrkrData->recordCount);
                    flushRet = flushBatchUnlocked(pWrkrData);
                    if (flushRet != RS_RET_OK) {
                        pWrkrData->pendingAsyncFlushRet = flushRet;
                    } else {
                        pWrkrData->pendingAsyncFlushRet = RS_RET_OK;
                    }
                    continue;
                }

                {
                    struct timespec timeout;
                    const long waitMs = (long)((uint64_t)pData->flushTimeoutMs - elapsed);
                    int condRet;
                    timeoutComp(&timeout, waitMs);
                    condRet = pthread_cond_timedwait(&pWrkrData->timerCond, &pWrkrData->batchLock, &timeout);
                    if (condRet != 0 && condRet != ETIMEDOUT) {
                        break;
                    }
                }
            }
        }

        (void)pthread_mutex_unlock(&pWrkrData->batchLock);
        DBGPRINTF("omazuredce[%p]: timer thread exiting\n", pWrkrData);
        return NULL;
    }

    static inline void setInstParamDefaults(instanceData * pData) {
        pData->templateName = NULL;
        pData->clientID = NULL;
        pData->clientSecret = NULL;
        pData->tenantID = NULL;
        pData->dceURL = NULL;
        pData->dcrID = NULL;
        pData->tableName = NULL;
        pData->accessToken = NULL;
        pData->maxBatchBytes = AZURE_MAX_BATCH_BYTES;
        pData->flushTimeoutMs = 1000;
    }

    BEGINbeginCnfLoad
        CODESTARTbeginCnfLoad;
        DBGPRINTF("omazuredce: beginCnfLoad\n");
    ENDbeginCnfLoad

    BEGINendCnfLoad
        CODESTARTendCnfLoad;
        DBGPRINTF("omazuredce: endCnfLoad\n");
    ENDendCnfLoad

    BEGINcheckCnf
        CODESTARTcheckCnf;
        DBGPRINTF("omazuredce: checkCnf\n");
    ENDcheckCnf

    BEGINactivateCnf
        CODESTARTactivateCnf;
        runModConf = pModConf;
        DBGPRINTF("omazuredce: activateCnf runModConf=%p\n", runModConf);
    ENDactivateCnf

    BEGINfreeCnf
        CODESTARTfreeCnf;
        DBGPRINTF("omazuredce: freeCnf\n");
    ENDfreeCnf

    BEGINcreateInstance
        int tokenLockInit = 0;
        CODESTARTcreateInstance;
        if (pthread_mutex_init(&pData->accessTokenLock, NULL) != 0) {
            ABORT_FINALIZE(RS_RET_SYS_ERR);
        }
        tokenLockInit = 1;
        pData->accessTokenLockInit = 1;
        DBGPRINTF("omazuredce: createInstance[%p]\n", pData);
    finalize_it:
        if (iRet != RS_RET_OK && tokenLockInit) {
            pthread_mutex_destroy(&pData->accessTokenLock);
            pData->accessTokenLockInit = 0;
        }
    ENDcreateInstance

    BEGINcreateWrkrInstance
        int mutexInit = 0;
        int condInit = 0;
        CODESTARTcreateWrkrInstance;
        DBGPRINTF("omazuredce: createWrkrInstance[%p] maxBatchBytes=%d flushTimeoutMs=%d\n", pWrkrData,
                  pWrkrData->pData->maxBatchBytes, pWrkrData->pData->flushTimeoutMs);
        pWrkrData->lastMessageTimeMs = nowMs();
        pWrkrData->timerThreadRunning = 0;
        pWrkrData->stopTimerThread = 0;
        pWrkrData->pendingAsyncFlushRet = RS_RET_OK;
        CHKmalloc(pWrkrData->batchBuf = malloc((size_t)pWrkrData->pData->maxBatchBytes + 1));
        if (pthread_mutex_init(&pWrkrData->batchLock, NULL) != 0) {
            ABORT_FINALIZE(RS_RET_SYS_ERR);
        }
        mutexInit = 1;
        if (pthread_cond_init(&pWrkrData->timerCond, NULL) != 0) {
            ABORT_FINALIZE(RS_RET_SYS_ERR);
        }
        condInit = 1;
        resetBatch(pWrkrData);
        CHKiRet(requestAccessToken(pWrkrData->pData));
        if (pthread_create(&pWrkrData->timerThread, NULL, batchTimerThread, pWrkrData) != 0) {
            ABORT_FINALIZE(RS_RET_SYS_ERR);
        }
        pWrkrData->timerThreadRunning = 1;
    finalize_it:
        if (iRet != RS_RET_OK) {
            if (pWrkrData->timerThreadRunning) {
                pWrkrData->stopTimerThread = 1;
                pthread_cond_signal(&pWrkrData->timerCond);
                pthread_join(pWrkrData->timerThread, NULL);
                pWrkrData->timerThreadRunning = 0;
            }
            if (condInit) pthread_cond_destroy(&pWrkrData->timerCond);
            if (mutexInit) pthread_mutex_destroy(&pWrkrData->batchLock);
            free(pWrkrData->batchBuf);
            pWrkrData->batchBuf = NULL;
        }
        DBGPRINTF("omazuredce: createWrkrInstance[%p] ret=%d\n", pWrkrData, iRet);
    ENDcreateWrkrInstance

    BEGINisCompatibleWithFeature
        CODESTARTisCompatibleWithFeature;
        if (eFeat == sFEATURERepeatedMsgReduction) iRet = RS_RET_OK;
    ENDisCompatibleWithFeature

    BEGINfreeInstance
        CODESTARTfreeInstance;
        DBGPRINTF("omazuredce: freeInstance[%p]\n", pData);
        free(pData->clientID);
        free(pData->clientSecret);
        free(pData->tenantID);
        free(pData->dceURL);
        free(pData->dcrID);
        free(pData->tableName);
        free(pData->accessToken);
        if (pData->accessTokenLockInit) pthread_mutex_destroy(&pData->accessTokenLock);
        free(pData->templateName);
    ENDfreeInstance

    BEGINfreeWrkrInstance
        CODESTARTfreeWrkrInstance;
        DBGPRINTF("omazuredce: freeWrkrInstance[%p]\n", pWrkrData);
        pWrkrData->stopTimerThread = 1;
        pthread_cond_signal(&pWrkrData->timerCond);
        if (pWrkrData->timerThreadRunning) {
            pthread_join(pWrkrData->timerThread, NULL);
            pWrkrData->timerThreadRunning = 0;
        }
        CHKiRet(flushBatch(pWrkrData));
    finalize_it:
        pthread_cond_destroy(&pWrkrData->timerCond);
        pthread_mutex_destroy(&pWrkrData->batchLock);
        free(pWrkrData->batchBuf);
    ENDfreeWrkrInstance

    BEGINdbgPrintInstInfo
        CODESTARTdbgPrintInstInfo;
        dbgprintf("omazuredce\n");
        dbgprintf("\ttemplate='%s'\n", safeStr(pData->templateName));
        dbgprintf("\tclient_id='%s'\n", safeStr(pData->clientID));
        dbgprintf("\ttenant_id='%s'\n", safeStr(pData->tenantID));
        dbgprintf("\tdce_url='%s'\n", safeStr(pData->dceURL));
        dbgprintf("\tdcr_id='%s'\n", safeStr(pData->dcrID));
        dbgprintf("\ttable_name='%s'\n", safeStr(pData->tableName));
        dbgprintf("\tmax_batch_bytes='%d'\n", pData->maxBatchBytes);
        dbgprintf("\tflush_timeout_ms='%d'\n", pData->flushTimeoutMs);
        dbgprintf("\taccess_token=%s\n", pData->accessToken == NULL ? "<unset>" : "<set>");
    ENDdbgPrintInstInfo

    BEGINtryResume
        int lockHeld = 0;
        CODESTARTtryResume;
        DBGPRINTF("omazuredce[%p]: tryResume\n", pWrkrData);
        if (pthread_mutex_lock(&pWrkrData->batchLock) != 0) {
            ABORT_FINALIZE(RS_RET_SYS_ERR);
        }
        lockHeld = 1;
        CHKiRet(retryPendingAsyncFlushUnlocked(pWrkrData));
    finalize_it:
        if (lockHeld) {
            (void)pthread_mutex_unlock(&pWrkrData->batchLock);
        }
    ENDtryResume

    BEGINbeginTransaction
        CODESTARTbeginTransaction;
        DBGPRINTF("omazuredce[%p]: beginTransaction\n", pWrkrData);
    ENDbeginTransaction

    BEGINdoAction
        const char *msg;
        size_t msgLen;
        int lockHeld = 0;
        size_t recCnt;
        CODESTARTdoAction;
        msg = (ppString != NULL) ? (const char *)ppString[0] : "";
        if (msg == NULL) msg = "";
        msgLen = strlen(msg);
        DBGPRINTF("omazuredce[%p]: doAction msgLen=%zu preview='%.*s%s'\n", pWrkrData, msgLen,
                  (int)(msgLen > 80 ? 80 : msgLen), msg, (msgLen > 80 ? "..." : ""));

        if (pthread_mutex_lock(&pWrkrData->batchLock) != 0) {
            ABORT_FINALIZE(RS_RET_SYS_ERR);
        }
        lockHeld = 1;
        CHKiRet(retryPendingAsyncFlushUnlocked(pWrkrData));
        CHKiRet(addMessageToBatchUnlocked(pWrkrData, msg));
        recCnt = pWrkrData->recordCount;
        pWrkrData->lastMessageTimeMs = nowMs();
        pthread_cond_signal(&pWrkrData->timerCond);
        if (pthread_mutex_unlock(&pWrkrData->batchLock) != 0) {
            lockHeld = 0;
            ABORT_FINALIZE(RS_RET_SYS_ERR);
        }
        lockHeld = 0;
        /* Signal queue engine that all previous records are already batched/flushed. */
        iRet = (recCnt == 1) ? RS_RET_PREVIOUS_COMMITTED : RS_RET_DEFER_COMMIT;
    finalize_it:
        if (lockHeld) {
            (void)pthread_mutex_unlock(&pWrkrData->batchLock);
        }
        DBGPRINTF("omazuredce[%p]: doAction ret=%d\n", pWrkrData, iRet);
    ENDdoAction

    BEGINendTransaction
        int lockHeld = 0;
        CODESTARTendTransaction;
        DBGPRINTF("omazuredce[%p]: endTransaction\n", pWrkrData);
        if (pthread_mutex_lock(&pWrkrData->batchLock) != 0) {
            ABORT_FINALIZE(RS_RET_SYS_ERR);
        }
        lockHeld = 1;
        CHKiRet(retryPendingAsyncFlushUnlocked(pWrkrData));
        /* Always flush at transaction end so queue commit never races ahead of HTTP delivery. */
        if (pWrkrData->recordCount > 0) {
            CHKiRet(flushBatchUnlocked(pWrkrData));
        }
        if (pthread_mutex_unlock(&pWrkrData->batchLock) != 0) {
            lockHeld = 0;
            ABORT_FINALIZE(RS_RET_SYS_ERR);
        }
        lockHeld = 0;
    finalize_it:
        if (lockHeld) {
            (void)pthread_mutex_unlock(&pWrkrData->batchLock);
        }
        DBGPRINTF("omazuredce[%p]: endTransaction ret=%d\n", pWrkrData, iRet);
    ENDendTransaction

    BEGINnewActInst
        struct cnfparamvals *pvals;
        int i;
        int nTpls;
        uchar *tplToUse;
        CODESTARTnewActInst;
        DBGPRINTF("omazuredce: newActInst begin\n");

        pvals = nvlstGetParams(lst, &actpblk, NULL);
        if (pvals == NULL) {
            LogError(0, RS_RET_MISSING_CNFPARAMS, "omazuredce: error reading config parameters");
            ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
        }

        CHKiRet(createInstance(&pData));
        setInstParamDefaults(pData);

        for (i = 0; i < actpblk.nParams; ++i) {
            if (!pvals[i].bUsed) continue;

            if (!strcmp(actpblk.descr[i].name, "template")) {
                free(pData->templateName);
                pData->templateName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            } else if (!strcmp(actpblk.descr[i].name, "client_id")) {
                pData->clientID = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            } else if (!strcmp(actpblk.descr[i].name, "client_secret")) {
                pData->clientSecret = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            } else if (!strcmp(actpblk.descr[i].name, "tenant_id")) {
                pData->tenantID = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            } else if (!strcmp(actpblk.descr[i].name, "dce_url")) {
                pData->dceURL = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            } else if (!strcmp(actpblk.descr[i].name, "dcr_id")) {
                pData->dcrID = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            } else if (!strcmp(actpblk.descr[i].name, "table_name")) {
                pData->tableName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
            } else if (!strcmp(actpblk.descr[i].name, "max_batch_bytes")) {
                pData->maxBatchBytes = (int)pvals[i].val.d.n;
            } else if (!strcmp(actpblk.descr[i].name, "flush_timeout_ms")) {
                pData->flushTimeoutMs = (int)pvals[i].val.d.n;
            }
        }
        DBGPRINTF(
            "omazuredce: parsed params template='%s' client_id='%s' tenant_id='%s' dce_url='%s' dcr_id='%s' "
            "table_name='%s' max_batch_bytes=%d flush_timeout_ms=%d client_secret=%s\n",
            safeStr(pData->templateName), safeStr(pData->clientID), safeStr(pData->tenantID), safeStr(pData->dceURL),
            safeStr(pData->dcrID), safeStr(pData->tableName), pData->maxBatchBytes, pData->flushTimeoutMs,
            (pData->clientSecret == NULL) ? "<unset>" : "<set>");

        if (pData->maxBatchBytes <= 0 || pData->maxBatchBytes > AZURE_MAX_BATCH_BYTES) {
            LogError(0, RS_RET_PARAM_ERROR, "omazuredce: max_batch_bytes must be in range 1..%d, got %d",
                     AZURE_MAX_BATCH_BYTES, pData->maxBatchBytes);
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }
        if (pData->clientID == NULL || pData->clientID[0] == '\0') {
            LogError(0, RS_RET_PARAM_ERROR, "omazuredce: parameter 'client_id' required but not specified");
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }
        if (pData->clientSecret == NULL || pData->clientSecret[0] == '\0') {
            LogError(0, RS_RET_PARAM_ERROR, "omazuredce: parameter 'client_secret' required but not specified");
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }
        if (pData->tenantID == NULL || pData->tenantID[0] == '\0') {
            LogError(0, RS_RET_PARAM_ERROR, "omazuredce: parameter 'tenant_id' required but not specified");
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }
        if (pData->dceURL == NULL || pData->dceURL[0] == '\0') {
            LogError(0, RS_RET_PARAM_ERROR, "omazuredce: parameter 'dce_url' required but not specified");
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }
        if (pData->dcrID == NULL || pData->dcrID[0] == '\0') {
            LogError(0, RS_RET_PARAM_ERROR, "omazuredce: parameter 'dcr_id' required but not specified");
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }
        if (pData->tableName == NULL || pData->tableName[0] == '\0') {
            LogError(0, RS_RET_PARAM_ERROR, "omazuredce: parameter 'table_name' required but not specified");
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }

        nTpls = 1;
        CODE_STD_STRING_REQUESTnewActInst(nTpls);
        CHKmalloc(tplToUse =
                      (uchar *)strdup((pData->templateName == NULL) ? " StdJSONFmt" : (char *)pData->templateName));
        CHKiRet(OMSRsetEntry(*ppOMSR, 0, tplToUse, OMSR_NO_RQD_TPL_OPTS));

        CODE_STD_FINALIZERnewActInst;
        cnfparamvalsDestruct(pvals, &actpblk);
    ENDnewActInst

    BEGINmodExit
        CODESTARTmodExit;
        DBGPRINTF("omazuredce: modExit\n");
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
            LogError(0, RS_RET_OBJ_CREATION_FAILED, "omazuredce: curl_global_init failed");
            ABORT_FINALIZE(RS_RET_OBJ_CREATION_FAILED);
        }
        DBGPRINTF("omazuredce: modInit complete\n");
    ENDmodInit

    /* vi:set ai:
     */
