/* imkubernetes.c
 * Kubernetes-native container log input module.
 *
 * This module tails Kubernetes pod/container log files from the host
 * filesystem, parses CRI or Docker json-file records, and optionally enriches
 * them with pod metadata from the Kubernetes API.
 *
 * Copyright (C) 2026 the rsyslog project.
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

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include "config.h"
#include "rsyslog.h"
#include <assert.h>
#include <ctype.h>
#include <curl/curl.h>
#include <errno.h>
#include <glob.h>
#include <json.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "cfsysline.h"
#include "datetime.h"
#include "debug.h"
#include "errmsg.h"
#include "glbl.h"
#include "module-template.h"
#include "msg.h"
#include "parser.h"
#include "prop.h"
#include "ratelimit.h"
#include "srUtils.h"
#include "statsobj.h"
#include "unicode-helper.h"

MODULE_TYPE_INPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("imkubernetes")

#define DFLT_LOG_FILE_GLOB "/var/log/pods/*/*/*.log"
#define DFLT_KUBERNETES_URL "https://kubernetes.default.svc.cluster.local:443"
#define DFLT_TOKEN_FILE "/var/run/secrets/kubernetes.io/serviceaccount/token"
#define DFLT_CA_CERT_FILE "/var/run/secrets/kubernetes.io/serviceaccount/ca.crt"
#define DFLT_pollingInterval 1
#define DFLT_cacheEntryTtl 300
#define DFLT_freshStartTail 0
#define DFLT_bEscapeLF 1
#define DFLT_enrichKubernetes 1
#define DFLT_SEVERITY pri2sev(LOG_INFO)
#define DFLT_FACILITY pri2fac(LOG_USER)

enum { dst_invalid = -1, dst_stdout, dst_stderr };

typedef struct partial_msg_s {
    uchar *data;
    size_t len;
    size_t cap;
    int stream_type;
    struct syslogTime st;
    time_t ttGenTime;
    sbool hasTime;
} partial_msg_t;

typedef struct path_meta_s {
    char *namespace_name;
    char *pod_name;
    char *pod_uid;
    char *container_name;
    char *container_id;
    int restart_count;
} path_meta_t;

typedef struct file_state_s {
    char *path;
    ino_t inode;
    off_t offset;
    sbool seen;
    path_meta_t meta;
    partial_msg_t partial;
    struct file_state_s *next;
} file_state_t;

typedef struct pod_cache_entry_s {
    char *key;
    time_t expires_at;
    struct json_object *json;
    struct pod_cache_entry_s *next;
} pod_cache_entry_t;

typedef struct curl_buf_s {
    char *data;
    size_t len;
    size_t cap;
} curl_buf_t;

typedef struct parsed_record_s {
    const uchar *msg;
    uchar *owned_msg;
    size_t len;
    int stream_type;
    struct syslogTime st;
    time_t ttGenTime;
    sbool hasTime;
    sbool is_partial;
    const char *format_name;
    sbool parse_error;
} parsed_record_t;

struct modConfData_s {
    rsconf_t *pConf;
    uchar *logFileGlob;
    int iPollInterval;
    int cacheEntryTtl;
    int iDfltSeverity;
    int iDfltFacility;
    sbool bEscapeLf;
    sbool freshStartTail;
    sbool enrichKubernetes;
    uchar *kubernetesUrl;
    uchar *token;
    uchar *tokenFile;
    uchar *caCertFile;
    sbool allowUnsignedCerts;
    sbool skipVerifyHost;
};

static modConfData_t *loadModConf = NULL;
static modConfData_t *runModConf = NULL;
static int bLegacyCnfModGlobalsPermitted;

DEF_IMOD_STATIC_DATA;
DEFobjCurrIf(glbl) DEFobjCurrIf(prop) DEFobjCurrIf(parser) DEFobjCurrIf(statsobj) DEFobjCurrIf(datetime)

    static prop_t *pInputName = NULL;
static ratelimit_t *ratelimiter = NULL;
static statsobj_t *modStats = NULL;
static file_state_t *g_fileStates = NULL;
static pod_cache_entry_t *g_podCache = NULL;
static CURL *g_apiCurl = NULL;
static struct curl_slist *g_apiHdr = NULL;
static char *gLineBuf = NULL;
static size_t gLineBufCap = 0;

STATSCOUNTER_DEF(ctrSubmitted, mutCtrSubmitted)
STATSCOUNTER_DEF(ctrParseErrors, mutCtrParseErrors)
STATSCOUNTER_DEF(ctrFilesDiscovered, mutCtrFilesDiscovered)
STATSCOUNTER_DEF(ctrCriRecords, mutCtrCriRecords)
STATSCOUNTER_DEF(ctrDockerJsonRecords, mutCtrDockerJsonRecords)
STATSCOUNTER_DEF(ctrCacheHits, mutCtrCacheHits)
STATSCOUNTER_DEF(ctrCacheMisses, mutCtrCacheMisses)
STATSCOUNTER_DEF(ctrApiErrors, mutCtrApiErrors)
STATSCOUNTER_DEF(ctrLostRatelimit, mutCtrLostRatelimit)

static struct cnfparamdescr modpdescr[] = {
    {"logfileglob", eCmdHdlrString, 0},        {"pollinginterval", eCmdHdlrPositiveInt, 0},
    {"cacheentryttl", eCmdHdlrPositiveInt, 0}, {"freshstarttail", eCmdHdlrBinary, 0},
    {"defaultseverity", eCmdHdlrSeverity, 0},  {"defaultfacility", eCmdHdlrFacility, 0},
    {"escapelf", eCmdHdlrBinary, 0},           {"enrichkubernetes", eCmdHdlrBinary, 0},
    {"kubernetesurl", eCmdHdlrString, 0},      {"token", eCmdHdlrString, 0},
    {"tokenfile", eCmdHdlrString, 0},          {"tls.cacert", eCmdHdlrString, 0},
    {"allowunsignedcerts", eCmdHdlrBinary, 0}, {"skipverifyhost", eCmdHdlrBinary, 0},
};
static struct cnfparamblk modpblk = {CNFPARAMBLK_VERSION, sizeof(modpdescr) / sizeof(struct cnfparamdescr), modpdescr};

static void freePathMeta(path_meta_t *meta) {
    if (meta == NULL) {
        return;
    }
    free(meta->namespace_name);
    free(meta->pod_name);
    free(meta->pod_uid);
    free(meta->container_name);
    free(meta->container_id);
    memset(meta, 0, sizeof(*meta));
    meta->restart_count = -1;
}

static void freePartialMsg(partial_msg_t *partial) {
    if (partial == NULL) {
        return;
    }
    free(partial->data);
    memset(partial, 0, sizeof(*partial));
    partial->stream_type = dst_invalid;
}

static void freeFileStates(void) {
    file_state_t *curr = g_fileStates;
    while (curr != NULL) {
        file_state_t *next = curr->next;
        free(curr->path);
        freePathMeta(&curr->meta);
        freePartialMsg(&curr->partial);
        free(curr);
        curr = next;
    }
    g_fileStates = NULL;
}

static void freePodCache(void) {
    pod_cache_entry_t *curr = g_podCache;
    while (curr != NULL) {
        pod_cache_entry_t *next = curr->next;
        free(curr->key);
        if (curr->json != NULL) {
            json_object_put(curr->json);
        }
        free(curr);
        curr = next;
    }
    g_podCache = NULL;
}

static char *dupRange(const char *start, size_t len) {
    char *out = NULL;
    if (start == NULL) {
        return NULL;
    }
    out = malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

static rsRetVal partialAppend(partial_msg_t *partial, const uchar *msg, size_t len) {
    DEFiRet;
    size_t needed;
    uchar *newbuf = NULL;

    assert(partial != NULL);
    if (msg == NULL || len == 0) {
        FINALIZE;
    }

    needed = partial->len + len + 1;
    if (needed > partial->cap) {
        size_t newcap = partial->cap == 0 ? 256 : partial->cap;
        while (newcap < needed) {
            newcap *= 2;
        }
        CHKmalloc(newbuf = realloc(partial->data, newcap));
        partial->data = newbuf;
        partial->cap = newcap;
    }

    memcpy(partial->data + partial->len, msg, len);
    partial->len += len;
    partial->data[partial->len] = '\0';

finalize_it:
    RETiRet;
}

static void trimTrailingNewline(const char *line, size_t *len) {
    while (*len > 0 && (line[*len - 1] == '\n' || line[*len - 1] == '\r')) {
        --(*len);
    }
}

static rsRetVal parseTimestamp3339(const char *ts, size_t len, struct syslogTime *st, time_t *ttGenTime, sbool *ok) {
    DEFiRet;
    uchar *mutableTs = NULL;
    uchar *parsePtr = NULL;
    int parseLen;

    assert(st != NULL);
    assert(ttGenTime != NULL);
    assert(ok != NULL);
    *ok = 0;
    *ttGenTime = 0;

    CHKmalloc(mutableTs = (uchar *)dupRange(ts, len));
    parsePtr = mutableTs;
    parseLen = (int)len;
    if (datetime.ParseTIMESTAMP3339(st, &parsePtr, &parseLen) == RS_RET_OK) {
        *ttGenTime = datetime.syslogTime2time_t(st);
        *ok = 1;
    }

finalize_it:
    free(mutableTs);
    RETiRet;
}

static sbool parsePodsPath(const char *path, path_meta_t *meta) {
    const char *filePart;
    const char *containerStart;
    const char *containerEnd;
    const char *podStart;
    const char *podEnd;
    const char *podsDirStart;
    const char *podsDirEnd;
    const char *u1;
    const char *u2;
    char *restartStr = NULL;
    char *restartEnd = NULL;

    filePart = strrchr(path, '/');
    if (filePart == NULL || filePart == path) {
        return 0;
    }
    containerEnd = filePart;
    containerStart = containerEnd;
    while (containerStart > path && containerStart[-1] != '/') {
        --containerStart;
    }
    if (containerStart == path) {
        return 0;
    }
    podEnd = containerStart - 1;
    podStart = podEnd;
    while (podStart > path && podStart[-1] != '/') {
        --podStart;
    }
    if (podStart == path) {
        return 0;
    }
    podsDirEnd = podStart - 1;
    podsDirStart = podsDirEnd;
    while (podsDirStart > path && podsDirStart[-1] != '/') {
        --podsDirStart;
    }
    if ((size_t)(podsDirEnd - podsDirStart) != sizeof("pods") - 1 ||
        strncmp(podsDirStart, "pods", sizeof("pods") - 1)) {
        return 0;
    }
    u1 = memchr(podStart, '_', (size_t)(podEnd - podStart));
    if (u1 == NULL) {
        return 0;
    }
    u2 = memchr(u1 + 1, '_', (size_t)(podEnd - (u1 + 1)));
    if (u2 == NULL) {
        return 0;
    }

    meta->namespace_name = dupRange(podStart, (size_t)(u1 - podStart));
    meta->pod_name = dupRange(u1 + 1, (size_t)(u2 - (u1 + 1)));
    meta->pod_uid = dupRange(u2 + 1, (size_t)(podEnd - (u2 + 1)));
    meta->container_name = dupRange(containerStart, (size_t)(containerEnd - containerStart));
    if (meta->namespace_name == NULL || meta->pod_name == NULL || meta->pod_uid == NULL ||
        meta->container_name == NULL) {
        return 0;
    }

    restartStr = dupRange(filePart + 1, strlen(filePart + 1));
    if (restartStr != NULL) {
        char *dot = strrchr(restartStr, '.');
        if (dot != NULL) {
            *dot = '\0';
        }
        errno = 0;
        meta->restart_count = (int)strtol(restartStr, &restartEnd, 10);
        if (errno != 0 || restartEnd == restartStr || *restartEnd != '\0') {
            free(restartStr);
            return 0;
        }
    }
    free(restartStr);
    return 1;
}

static sbool parseContainersPath(const char *path, path_meta_t *meta) {
    const char *filePart = strrchr(path, '/');
    char *base = NULL;
    char *dot = NULL;
    char *dash = NULL;
    char *u1 = NULL;
    char *u2 = NULL;

    if (filePart == NULL) {
        filePart = path;
    } else {
        ++filePart;
    }
    base = strdup(filePart);
    if (base == NULL) {
        return 0;
    }
    dot = strrchr(base, '.');
    if (dot != NULL) {
        *dot = '\0';
    }
    dash = strrchr(base, '-');
    if (dash == NULL || dash == base || dash[1] == '\0') {
        free(base);
        return 0;
    }
    *dash = '\0';
    u1 = strchr(base, '_');
    if (u1 == NULL) {
        free(base);
        return 0;
    }
    u2 = strchr(u1 + 1, '_');
    if (u2 == NULL) {
        free(base);
        return 0;
    }

    meta->pod_name = dupRange(base, (size_t)(u1 - base));
    meta->namespace_name = dupRange(u1 + 1, (size_t)(u2 - (u1 + 1)));
    meta->container_name = strdup(u2 + 1);
    meta->container_id = strdup(dash + 1);
    meta->restart_count = -1;
    free(base);
    return meta->pod_name != NULL && meta->namespace_name != NULL && meta->container_name != NULL &&
           meta->container_id != NULL;
}

static sbool populatePathMeta(const char *path, path_meta_t *meta) {
    const char *containersSeg = strstr(path, "/containers/");
    const char *podsSeg = strstr(path, "/pods/");

    memset(meta, 0, sizeof(*meta));
    meta->restart_count = -1;
    if (containersSeg != NULL) {
        if (parseContainersPath(path, meta)) {
            return 1;
        }
        freePathMeta(meta);
    }
    if (podsSeg != NULL) {
        if (parsePodsPath(path, meta)) {
            return 1;
        }
        freePathMeta(meta);
    }
    if (parsePodsPath(path, meta)) {
        return 1;
    }
    freePathMeta(meta);
    if (parseContainersPath(path, meta)) {
        return 1;
    }
    freePathMeta(meta);
    return 0;
}

static file_state_t *findFileState(const char *path) {
    file_state_t *curr;
    for (curr = g_fileStates; curr != NULL; curr = curr->next) {
        if (!strcmp(curr->path, path)) {
            return curr;
        }
    }
    return NULL;
}

static file_state_t *createFileState(const char *path) {
    file_state_t *state = NULL;
    state = calloc(1, sizeof(*state));
    if (state == NULL) {
        return NULL;
    }
    state->path = strdup(path);
    if (state->path == NULL) {
        free(state);
        return NULL;
    }
    state->offset = 0;
    state->inode = 0;
    state->seen = 1;
    state->partial.stream_type = dst_invalid;
    if (!populatePathMeta(path, &state->meta)) {
        LogError(0, RS_RET_ERR, "imkubernetes: could not parse kubernetes metadata from path '%s'", path);
        STATSCOUNTER_INC(ctrParseErrors, mutCtrParseErrors);
    }
    state->next = g_fileStates;
    g_fileStates = state;
    STATSCOUNTER_INC(ctrFilesDiscovered, mutCtrFilesDiscovered);
    return state;
}

static void markAllFileStatesUnseen(void) {
    file_state_t *curr;
    for (curr = g_fileStates; curr != NULL; curr = curr->next) {
        curr->seen = 0;
    }
}

static void sweepFileStates(void) {
    file_state_t **cursor = &g_fileStates;
    while (*cursor != NULL) {
        file_state_t *curr = *cursor;
        if (curr->seen) {
            cursor = &curr->next;
            continue;
        }
        *cursor = curr->next;
        free(curr->path);
        freePathMeta(&curr->meta);
        freePartialMsg(&curr->partial);
        free(curr);
    }
}

static pod_cache_entry_t *findPodCacheEntry(const char *key, time_t now) {
    pod_cache_entry_t **cursor = &g_podCache;
    while (*cursor != NULL) {
        pod_cache_entry_t *curr = *cursor;
        if (curr->expires_at <= now) {
            *cursor = curr->next;
            free(curr->key);
            if (curr->json != NULL) {
                json_object_put(curr->json);
            }
            free(curr);
            continue;
        }
        if (!strcmp(curr->key, key)) {
            return curr;
        }
        cursor = &curr->next;
    }
    return NULL;
}

static rsRetVal setPodCacheEntry(const char *key, struct json_object *json, time_t now) {
    DEFiRet;
    pod_cache_entry_t *entry = NULL;

    assert(key != NULL);
    assert(json != NULL);

    if ((entry = findPodCacheEntry(key, now)) != NULL) {
        if (entry->json != NULL) {
            json_object_put(entry->json);
        }
        entry->json = json;
        entry->expires_at = now + runModConf->cacheEntryTtl;
        entry = NULL;
        FINALIZE;
    }

    CHKmalloc(entry = calloc(1, sizeof(*entry)));
    CHKmalloc(entry->key = strdup(key));
    entry->json = json;
    entry->expires_at = now + runModConf->cacheEntryTtl;
    entry->next = g_podCache;
    g_podCache = entry;
    entry = NULL;

finalize_it:
    if (entry != NULL) {
        free(entry->key);
        free(entry);
    }
    RETiRet;
}

static size_t apiCurlCB(void *data, size_t size, size_t nmemb, void *buffer) {
    curl_buf_t *buf = (curl_buf_t *)buffer;
    size_t realsize = size * nmemb;
    size_t needed = buf->len + realsize + 1;
    char *newbuf;

    if (needed > buf->cap) {
        size_t newcap = buf->cap == 0 ? 1024 : buf->cap;
        while (newcap < needed) {
            newcap *= 2;
        }
        newbuf = realloc(buf->data, newcap);
        if (newbuf == NULL) {
            return 0;
        }
        buf->data = newbuf;
        buf->cap = newcap;
    }

    memcpy(buf->data + buf->len, data, realsize);
    buf->len += realsize;
    buf->data[buf->len] = '\0';
    return realsize;
}

static char *readFileTrimmed(const char *path) {
    FILE *fp = NULL;
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;

    fp = fopen(path, "r");
    if (fp == NULL) {
        return NULL;
    }
    len = getline(&line, &cap, fp);
    fclose(fp);
    if (len < 0) {
        free(line);
        return NULL;
    }
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }
    return line;
}

static void copyJsonField(struct json_object *dst, struct json_object *src, const char *field) {
    struct json_object *value = NULL;
    if (dst == NULL || src == NULL) {
        return;
    }
    if (json_object_object_get_ex(src, field, &value)) {
        json_object_object_add(dst, (char *)field, json_object_get(value));
    }
}

static rsRetVal initApiClient(void) {
    DEFiRet;
    CURLcode ccode;
    char *token = NULL;
    char authHeader[4096];

    if (!runModConf->enrichKubernetes) {
        FINALIZE;
    }

    if ((ccode = curl_global_init(CURL_GLOBAL_ALL)) != CURLE_OK) {
        LogError(0, RS_RET_ERR, "imkubernetes: curl_global_init failed: %s", curl_easy_strerror(ccode));
        ABORT_FINALIZE(RS_RET_ERR);
    }

    if (runModConf->token != NULL) {
        token = strdup((char *)runModConf->token);
    } else if (runModConf->tokenFile != NULL) {
        token = readFileTrimmed((char *)runModConf->tokenFile);
    }

    g_apiCurl = curl_easy_init();
    if (g_apiCurl == NULL) {
        LogError(0, RS_RET_ERR, "imkubernetes: curl_easy_init failed");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    if (token != NULL && token[0] != '\0') {
        snprintf(authHeader, sizeof(authHeader), "Authorization: Bearer %s", token);
        g_apiHdr = curl_slist_append(g_apiHdr, authHeader);
    }

    curl_easy_setopt(g_apiCurl, CURLOPT_HTTPHEADER, g_apiHdr);
    curl_easy_setopt(g_apiCurl, CURLOPT_WRITEFUNCTION, apiCurlCB);
    curl_easy_setopt(g_apiCurl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(g_apiCurl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(g_apiCurl, CURLOPT_USERAGENT, "rsyslog/imkubernetes");
    if (runModConf->caCertFile != NULL) {
        curl_easy_setopt(g_apiCurl, CURLOPT_CAINFO, runModConf->caCertFile);
    }
    if (runModConf->allowUnsignedCerts) {
        curl_easy_setopt(g_apiCurl, CURLOPT_SSL_VERIFYPEER, 0L);
    }
    if (runModConf->skipVerifyHost) {
        curl_easy_setopt(g_apiCurl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

finalize_it:
    free(token);
    RETiRet;
}

static void freeApiClient(void) {
    if (g_apiCurl != NULL) {
        curl_easy_cleanup(g_apiCurl);
        g_apiCurl = NULL;
    }
    if (g_apiHdr != NULL) {
        curl_slist_free_all(g_apiHdr);
        g_apiHdr = NULL;
    }
    if (runModConf != NULL && runModConf->enrichKubernetes) {
        curl_global_cleanup();
    }
}

static rsRetVal queryPodMetadata(const path_meta_t *meta, struct json_object **result) {
    DEFiRet;
    char *url = NULL;
    curl_buf_t buf = {0};
    CURLcode ccode;
    long respCode = 0;
    struct json_object *reply = NULL;
    struct json_object *metadata = NULL;
    struct json_object *status = NULL;
    struct json_object *ownerRefs = NULL;
    struct json_object *ownerRef = NULL;
    struct json_object *out = NULL;
    struct json_object *value = NULL;

    assert(result != NULL);
    *result = NULL;

    if (g_apiCurl == NULL || meta == NULL || meta->namespace_name == NULL || meta->pod_name == NULL) {
        FINALIZE;
    }

    if (asprintf(&url, "%s/api/v1/namespaces/%s/pods/%s", (char *)runModConf->kubernetesUrl, meta->namespace_name,
                 meta->pod_name) == -1) {
        url = NULL;
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    curl_easy_setopt(g_apiCurl, CURLOPT_URL, url);
    curl_easy_setopt(g_apiCurl, CURLOPT_WRITEDATA, &buf);
    ccode = curl_easy_perform(g_apiCurl);
    if (ccode != CURLE_OK) {
        LogError(0, RS_RET_ERR, "imkubernetes: kubernetes API request failed for '%s': %s", url,
                 curl_easy_strerror(ccode));
        STATSCOUNTER_INC(ctrApiErrors, mutCtrApiErrors);
        ABORT_FINALIZE(RS_RET_ERR);
    }

    ccode = curl_easy_getinfo(g_apiCurl, CURLINFO_RESPONSE_CODE, &respCode);
    if (ccode != CURLE_OK) {
        LogError(0, RS_RET_ERR, "imkubernetes: curl_easy_getinfo failed for '%s': %s", url, curl_easy_strerror(ccode));
        STATSCOUNTER_INC(ctrApiErrors, mutCtrApiErrors);
        ABORT_FINALIZE(RS_RET_ERR);
    }

    if (respCode == 404) {
        ABORT_FINALIZE(RS_RET_NOT_FOUND);
    }
    if (respCode != 200) {
        LogError(0, RS_RET_ERR, "imkubernetes: kubernetes API returned %ld for '%s'", respCode, url);
        STATSCOUNTER_INC(ctrApiErrors, mutCtrApiErrors);
        ABORT_FINALIZE(RS_RET_ERR);
    }

    reply = json_tokener_parse(buf.data != NULL ? buf.data : "{}");
    if (reply == NULL) {
        STATSCOUNTER_INC(ctrApiErrors, mutCtrApiErrors);
        ABORT_FINALIZE(RS_RET_JSON_PARSE_ERR);
    }

    CHKmalloc(out = json_object_new_object());
    if (json_object_object_get_ex(reply, "metadata", &metadata)) {
        copyJsonField(out, metadata, "uid");
        if (json_object_object_get_ex(metadata, "uid", &value)) {
            json_object_object_del(out, "uid");
            json_object_object_add(out, "pod_id", json_object_get(value));
        }
        copyJsonField(out, metadata, "creationTimestamp");
        if (json_object_object_get_ex(metadata, "creationTimestamp", &value)) {
            json_object_object_del(out, "creationTimestamp");
            json_object_object_add(out, "creation_timestamp", json_object_get(value));
        }
        copyJsonField(out, metadata, "labels");
        copyJsonField(out, metadata, "annotations");
        if (json_object_object_get_ex(metadata, "ownerReferences", &ownerRefs) &&
            json_object_is_type(ownerRefs, json_type_array) && json_object_array_length(ownerRefs) > 0) {
            ownerRef = json_object_array_get_idx(ownerRefs, 0);
            copyJsonField(out, ownerRef, "kind");
            if (json_object_object_get_ex(ownerRef, "kind", &value)) {
                json_object_object_del(out, "kind");
                json_object_object_add(out, "owner_kind", json_object_get(value));
            }
            copyJsonField(out, ownerRef, "name");
            if (json_object_object_get_ex(ownerRef, "name", &value)) {
                json_object_object_del(out, "name");
                json_object_object_add(out, "owner_name", json_object_get(value));
            }
        }
    }
    if (json_object_object_get_ex(reply, "status", &status)) {
        copyJsonField(out, status, "podIP");
        if (json_object_object_get_ex(status, "podIP", &value)) {
            json_object_object_del(out, "podIP");
            json_object_object_add(out, "pod_ip", json_object_get(value));
        }
        copyJsonField(out, status, "hostIP");
        if (json_object_object_get_ex(status, "hostIP", &value)) {
            json_object_object_del(out, "hostIP");
            json_object_object_add(out, "host_ip", json_object_get(value));
        }
    }
    if (json_object_object_get_ex(reply, "spec", &status)) {
        if (json_object_object_get_ex(status, "nodeName", &value)) {
            json_object_object_add(out, "host", json_object_get(value));
        }
    }
    json_object_object_add(out, "master_url", json_object_new_string((char *)runModConf->kubernetesUrl));
    *result = out;
    out = NULL;

finalize_it:
    free(url);
    free(buf.data);
    if (reply != NULL) {
        json_object_put(reply);
    }
    if (out != NULL) {
        json_object_put(out);
    }
    RETiRet;
}

static struct json_object *getPodMetadataForState(const path_meta_t *meta) {
    char *key = NULL;
    time_t now;
    struct json_object *cached = NULL;
    struct json_object *fresh = NULL;
    rsRetVal iRet;

    if (!runModConf->enrichKubernetes || meta == NULL || meta->namespace_name == NULL || meta->pod_name == NULL) {
        return NULL;
    }

    datetime.GetTime(&now);
    if (asprintf(&key, "%s/%s", meta->namespace_name, meta->pod_name) == -1) {
        key = NULL;
        return NULL;
    }

    if ((cached = (findPodCacheEntry(key, now) != NULL ? findPodCacheEntry(key, now)->json : NULL)) != NULL) {
        STATSCOUNTER_INC(ctrCacheHits, mutCtrCacheHits);
        free(key);
        return cached;
    }

    STATSCOUNTER_INC(ctrCacheMisses, mutCtrCacheMisses);
    iRet = queryPodMetadata(meta, &fresh);
    if (iRet == RS_RET_OK && fresh != NULL) {
        if (setPodCacheEntry(key, fresh, now) == RS_RET_OK) {
            free(key);
            return fresh;
        }
        json_object_put(fresh);
    }
    free(key);
    return NULL;
}

static rsRetVal addRuntimeMetadata(smsg_t *pMsg, const file_state_t *state, const parsed_record_t *record) {
    struct json_object *k8s = NULL;
    struct json_object *docker = NULL;
    struct json_object *podMd = NULL;
    struct json_object *value = NULL;
    DEFiRet;

    assert(pMsg != NULL);
    assert(state != NULL);
    assert(record != NULL);

    CHKmalloc(k8s = json_object_new_object());
    if (state->meta.namespace_name != NULL) {
        json_object_object_add(k8s, "namespace_name", json_object_new_string(state->meta.namespace_name));
    }
    if (state->meta.pod_name != NULL) {
        json_object_object_add(k8s, "pod_name", json_object_new_string(state->meta.pod_name));
    }
    if (state->meta.pod_uid != NULL) {
        json_object_object_add(k8s, "pod_uid", json_object_new_string(state->meta.pod_uid));
    }
    if (state->meta.container_name != NULL) {
        json_object_object_add(k8s, "container_name", json_object_new_string(state->meta.container_name));
    }
    if (state->meta.restart_count >= 0) {
        json_object_object_add(k8s, "restart_count", json_object_new_int(state->meta.restart_count));
    }
    json_object_object_add(k8s, "log_file", json_object_new_string(state->path));
    json_object_object_add(k8s, "stream",
                           json_object_new_string(record->stream_type == dst_stderr ? "stderr" : "stdout"));
    json_object_object_add(k8s, "log_format", json_object_new_string(record->format_name));
    if (record->parse_error) {
        json_object_object_add(k8s, "parse_error", json_object_new_boolean(1));
    }

    podMd = getPodMetadataForState(&state->meta);
    if (podMd != NULL) {
        copyJsonField(k8s, podMd, "pod_id");
        copyJsonField(k8s, podMd, "creation_timestamp");
        copyJsonField(k8s, podMd, "labels");
        copyJsonField(k8s, podMd, "annotations");
        copyJsonField(k8s, podMd, "owner_kind");
        copyJsonField(k8s, podMd, "owner_name");
        copyJsonField(k8s, podMd, "pod_ip");
        copyJsonField(k8s, podMd, "host_ip");
        copyJsonField(k8s, podMd, "master_url");
        if (json_object_object_get_ex(podMd, "host", &value)) {
            json_object_object_add(k8s, "host", json_object_get(value));
        }
    }

    CHKmalloc(docker = json_object_new_object());
    if (state->meta.container_id != NULL) {
        json_object_object_add(docker, "container_id", json_object_new_string(state->meta.container_id));
    }

    CHKiRet(msgAddJSON(pMsg, (uchar *)"!kubernetes", k8s, 0, 0));
    k8s = NULL;
    CHKiRet(msgAddJSON(pMsg, (uchar *)"!docker", docker, 0, 0));
    docker = NULL;

finalize_it:
    if (k8s != NULL) {
        json_object_put(k8s);
    }
    if (docker != NULL) {
        json_object_put(docker);
    }
    RETiRet;
}

static rsRetVal enqMsg(const file_state_t *state, const parsed_record_t *record) {
    smsg_t *pMsg = NULL;
    DEFiRet;

    if (record == NULL || record->msg == NULL) {
        FINALIZE;
    }

    if (record->hasTime) {
        CHKiRet(msgConstructWithTime(&pMsg, &record->st, record->ttGenTime));
    } else {
        CHKiRet(msgConstruct(&pMsg));
    }

    MsgSetFlowControlType(pMsg, eFLOWCTL_LIGHT_DELAY);
    MsgSetInputName(pMsg, pInputName);
    MsgSetRawMsg(pMsg, (const char *)record->msg, record->len);

    if (runModConf->bEscapeLf) {
        parser.SanitizeMsg(pMsg);
    } else {
        if (pMsg->iLenRawMsg > 0 && pMsg->pszRawMsg[pMsg->iLenRawMsg - 1] == '\0') {
            pMsg->iLenRawMsg--;
        }
    }

    MsgSetMSGoffs(pMsg, 0);
    MsgSetRcvFrom(pMsg, glbl.GetLocalHostNameProp());
    MsgSetRcvFromIP(pMsg, glbl.GetLocalHostIP());
    MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
    MsgSetTAG(pMsg, UCHAR_CONSTANT("kubernetes:"), sizeof("kubernetes:") - 1);
    pMsg->iFacility = runModConf->iDfltFacility;
    pMsg->iSeverity = (record->stream_type == dst_stderr) ? LOG_ERR : runModConf->iDfltSeverity;

    CHKiRet(addRuntimeMetadata(pMsg, state, record));
    CHKiRet(ratelimitAddMsg(ratelimiter, NULL, pMsg));
    STATSCOUNTER_INC(ctrSubmitted, mutCtrSubmitted);

finalize_it:
    if (iRet == RS_RET_DISCARDMSG) {
        STATSCOUNTER_INC(ctrLostRatelimit, mutCtrLostRatelimit);
    }
    RETiRet;
}

static rsRetVal emitPartialIfComplete(file_state_t *state, const parsed_record_t *record) {
    parsed_record_t finalRecord = {0};
    DEFiRet;

    if (!record->is_partial && state->partial.len == 0) {
        CHKiRet(enqMsg(state, record));
        FINALIZE;
    }

    if (record->is_partial && state->partial.len == 0) {
        state->partial.stream_type = record->stream_type;
        state->partial.hasTime = record->hasTime;
        state->partial.ttGenTime = record->ttGenTime;
        if (record->hasTime) {
            memcpy(&state->partial.st, &record->st, sizeof(record->st));
        }
    }

    if (state->partial.len > 0 && state->partial.stream_type != record->stream_type) {
        freePartialMsg(&state->partial);
        state->partial.stream_type = record->stream_type;
        state->partial.hasTime = record->hasTime;
        state->partial.ttGenTime = record->ttGenTime;
        if (record->hasTime) {
            memcpy(&state->partial.st, &record->st, sizeof(record->st));
        }
    }

    if (record->is_partial) {
        CHKiRet(partialAppend(&state->partial, record->msg, record->len));
        FINALIZE;
    }

    if (state->partial.len > 0) {
        CHKiRet(partialAppend(&state->partial, record->msg, record->len));
        finalRecord.msg = state->partial.data;
        finalRecord.len = state->partial.len;
        finalRecord.stream_type = state->partial.stream_type;
        finalRecord.hasTime = state->partial.hasTime;
        finalRecord.ttGenTime = state->partial.ttGenTime;
        finalRecord.format_name = record->format_name;
        finalRecord.parse_error = record->parse_error;
        if (finalRecord.hasTime) {
            memcpy(&finalRecord.st, &state->partial.st, sizeof(finalRecord.st));
        }
        CHKiRet(enqMsg(state, &finalRecord));
        freePartialMsg(&state->partial);
        FINALIZE;
    }

    CHKiRet(enqMsg(state, record));

finalize_it:
    RETiRet;
}

static sbool parseCriLine(const char *line, size_t len, parsed_record_t *record) {
    const char *sp1;
    const char *sp2;
    const char *sp3;

    sp1 = memchr(line, ' ', len);
    if (sp1 == NULL) {
        return 0;
    }
    sp2 = memchr(sp1 + 1, ' ', len - (size_t)(sp1 + 1 - line));
    if (sp2 == NULL) {
        return 0;
    }
    sp3 = memchr(sp2 + 1, ' ', len - (size_t)(sp2 + 1 - line));
    if (sp3 == NULL || sp3 <= sp2 + 1) {
        return 0;
    }
    if (strncmp(sp1 + 1, "stdout", (size_t)(sp2 - (sp1 + 1))) &&
        strncmp(sp1 + 1, "stderr", (size_t)(sp2 - (sp1 + 1)))) {
        return 0;
    }
    if ((size_t)(sp2 - (sp1 + 1)) != 6) {
        return 0;
    }
    record->stream_type = !strncmp(sp1 + 1, "stderr", 6) ? dst_stderr : dst_stdout;
    record->is_partial = (*(sp2 + 1) == 'P');
    record->msg = (const uchar *)(sp3 + 1);
    record->len = len - (size_t)(sp3 + 1 - line);
    record->format_name = "cri";
    parseTimestamp3339(line, (size_t)(sp1 - line), &record->st, &record->ttGenTime, &record->hasTime);
    return 1;
}

static sbool parseDockerJsonLine(const char *line, size_t len, parsed_record_t *record) {
    struct json_object *json = NULL;
    struct json_object *logObj = NULL;
    struct json_object *streamObj = NULL;
    struct json_object *timeObj = NULL;
    const char *msg = NULL;
    size_t msgLen = 0;

    (void)len;
    json = json_tokener_parse(line);
    if (json == NULL) {
        return 0;
    }
    if (!json_object_object_get_ex(json, "log", &logObj) || !json_object_is_type(logObj, json_type_string)) {
        json_object_put(json);
        return 0;
    }

    msg = json_object_get_string(logObj);
    msgLen = strlen(msg);
    if (msgLen > 0 && msg[msgLen - 1] == '\n') {
        --msgLen;
    }

    record->owned_msg = (uchar *)dupRange(msg, msgLen);
    if (record->owned_msg == NULL) {
        json_object_put(json);
        return 0;
    }
    record->msg = record->owned_msg;
    record->len = msgLen;
    record->stream_type = dst_stdout;
    record->hasTime = 0;
    record->is_partial = 0;
    record->format_name = "docker_json";
    if (json_object_object_get_ex(json, "stream", &streamObj) && json_object_is_type(streamObj, json_type_string)) {
        const char *stream = json_object_get_string(streamObj);
        if (stream != NULL && !strcmp(stream, "stderr")) {
            record->stream_type = dst_stderr;
        }
    }
    if (json_object_object_get_ex(json, "time", &timeObj) && json_object_is_type(timeObj, json_type_string)) {
        const char *timeStr = json_object_get_string(timeObj);
        if (timeStr != NULL) {
            parseTimestamp3339(timeStr, strlen(timeStr), &record->st, &record->ttGenTime, &record->hasTime);
        }
    }

    json_object_put(json);
    return 1;
}

static rsRetVal processLine(file_state_t *state, const char *line, size_t len) {
    parsed_record_t record = {0};
    DEFiRet;

    trimTrailingNewline(line, &len);
    if (len == 0) {
        FINALIZE;
    }

    if (parseCriLine(line, len, &record)) {
        STATSCOUNTER_INC(ctrCriRecords, mutCtrCriRecords);
        CHKiRet(emitPartialIfComplete(state, &record));
        FINALIZE;
    }

    if (parseDockerJsonLine(line, len, &record)) {
        STATSCOUNTER_INC(ctrDockerJsonRecords, mutCtrDockerJsonRecords);
        CHKiRet(emitPartialIfComplete(state, &record));
        FINALIZE;
    }

    record.msg = (const uchar *)line;
    record.len = len;
    record.stream_type = dst_stdout;
    record.hasTime = 0;
    record.is_partial = 0;
    record.format_name = "raw";
    record.parse_error = 1;
    STATSCOUNTER_INC(ctrParseErrors, mutCtrParseErrors);
    CHKiRet(emitPartialIfComplete(state, &record));

finalize_it:
    free(record.owned_msg);
    RETiRet;
}

static rsRetVal processFileState(file_state_t *state) {
    DEFiRet;
    FILE *fp = NULL;
    ssize_t readLen;
    struct stat st;

    assert(state != NULL);

    if (stat(state->path, &st) != 0) {
        FINALIZE;
    }

    if (state->inode != 0 && (state->inode != st.st_ino || st.st_size < state->offset)) {
        state->offset = 0;
        freePartialMsg(&state->partial);
    }
    state->inode = st.st_ino;

    if (state->offset == 0 && runModConf->freshStartTail && st.st_size > 0) {
        state->offset = st.st_size;
        FINALIZE;
    }
    if (st.st_size <= state->offset) {
        FINALIZE;
    }

    fp = fopen(state->path, "r");
    if (fp == NULL) {
        ABORT_FINALIZE(RS_RET_NO_FILE_ACCESS);
    }
    if (fseeko(fp, state->offset, SEEK_SET) != 0) {
        ABORT_FINALIZE(RS_RET_ERR);
    }

    while ((readLen = getline(&gLineBuf, &gLineBufCap, fp)) >= 0) {
        CHKiRet(processLine(state, gLineBuf, (size_t)readLen));
    }
    state->offset = ftello(fp);

finalize_it:
    if (fp != NULL) {
        fclose(fp);
    }
    RETiRet;
}

static rsRetVal scanAndProcessFiles(void) {
    DEFiRet;
    glob_t matches;
    int gret;
    size_t i;

    memset(&matches, 0, sizeof(matches));
    markAllFileStatesUnseen();

    gret = glob((char *)runModConf->logFileGlob, 0, NULL, &matches);
    if (gret != 0 && gret != GLOB_NOMATCH) {
        ABORT_FINALIZE(RS_RET_ERR);
    }

    for (i = 0; i < matches.gl_pathc; ++i) {
        file_state_t *state = findFileState(matches.gl_pathv[i]);
        if (state == NULL) {
            state = createFileState(matches.gl_pathv[i]);
            if (state == NULL) {
                continue;
            }
        }
        state->seen = 1;
        processFileState(state);
    }

    sweepFileStates();

finalize_it:
    globfree(&matches);
    RETiRet;
}

BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
    pModConf->logFileGlob = NULL;
    pModConf->iPollInterval = DFLT_pollingInterval;
    pModConf->cacheEntryTtl = DFLT_cacheEntryTtl;
    pModConf->iDfltSeverity = DFLT_SEVERITY;
    pModConf->iDfltFacility = DFLT_FACILITY;
    pModConf->bEscapeLf = DFLT_bEscapeLF;
    pModConf->freshStartTail = DFLT_freshStartTail;
    pModConf->enrichKubernetes = DFLT_enrichKubernetes;
    pModConf->kubernetesUrl = NULL;
    pModConf->token = NULL;
    pModConf->tokenFile = NULL;
    pModConf->caCertFile = NULL;
    pModConf->allowUnsignedCerts = 0;
    pModConf->skipVerifyHost = 0;
    bLegacyCnfModGlobalsPermitted = 1;
ENDbeginCnfLoad

BEGINsetModCnf
    struct cnfparamvals *pvals = NULL;
    int i;
    CODESTARTsetModCnf;
    pvals = nvlstGetParams(lst, &modpblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS, "imkubernetes: error processing module config parameters");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }
    if (Debug) {
        dbgprintf("module (global) param blk for imkubernetes:\n");
        cnfparamsPrint(&modpblk, pvals);
    }
    for (i = 0; i < modpblk.nParams; ++i) {
        if (!pvals[i].bUsed) {
            continue;
        } else if (!strcmp(modpblk.descr[i].name, "logfileglob")) {
            free(loadModConf->logFileGlob);
            loadModConf->logFileGlob = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "pollinginterval")) {
            loadModConf->iPollInterval = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "cacheentryttl")) {
            loadModConf->cacheEntryTtl = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "freshstarttail")) {
            loadModConf->freshStartTail = (sbool)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "defaultseverity")) {
            loadModConf->iDfltSeverity = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "defaultfacility")) {
            loadModConf->iDfltFacility = (int)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "escapelf")) {
            loadModConf->bEscapeLf = (sbool)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "enrichkubernetes")) {
            loadModConf->enrichKubernetes = (sbool)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "kubernetesurl")) {
            free(loadModConf->kubernetesUrl);
            loadModConf->kubernetesUrl = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "token")) {
            free(loadModConf->token);
            loadModConf->token = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "tokenfile")) {
            free(loadModConf->tokenFile);
            loadModConf->tokenFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "tls.cacert")) {
            free(loadModConf->caCertFile);
            loadModConf->caCertFile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(modpblk.descr[i].name, "allowunsignedcerts")) {
            loadModConf->allowUnsignedCerts = (sbool)pvals[i].val.d.n;
        } else if (!strcmp(modpblk.descr[i].name, "skipverifyhost")) {
            loadModConf->skipVerifyHost = (sbool)pvals[i].val.d.n;
        }
    }
    bLegacyCnfModGlobalsPermitted = 0;
finalize_it:
    if (pvals != NULL) {
        cnfparamvalsDestruct(pvals, &modpblk);
    }
ENDsetModCnf

BEGINendCnfLoad
    CODESTARTendCnfLoad;
ENDendCnfLoad

BEGINcheckCnf
    CODESTARTcheckCnf;
    if (loadModConf->iPollInterval <= 0) {
        LogError(0, RS_RET_CONFIG_ERROR, "imkubernetes: PollingInterval must be greater than zero");
        iRet = RS_RET_CONFIG_ERROR;
    }
ENDcheckCnf

BEGINactivateCnf
    CODESTARTactivateCnf;
    if (loadModConf->logFileGlob == NULL) {
        CHKmalloc(loadModConf->logFileGlob = (uchar *)strdup(DFLT_LOG_FILE_GLOB));
    }
    if (loadModConf->kubernetesUrl == NULL) {
        CHKmalloc(loadModConf->kubernetesUrl = (uchar *)strdup(DFLT_KUBERNETES_URL));
    }
    if (loadModConf->tokenFile == NULL) {
        CHKmalloc(loadModConf->tokenFile = (uchar *)strdup(DFLT_TOKEN_FILE));
    }
    if (loadModConf->caCertFile == NULL) {
        CHKmalloc(loadModConf->caCertFile = (uchar *)strdup(DFLT_CA_CERT_FILE));
    }
    runModConf = loadModConf;

    CHKiRet(statsobj.Construct(&modStats));
    CHKiRet(statsobj.SetName(modStats, UCHAR_CONSTANT("imkubernetes")));
    CHKiRet(statsobj.SetOrigin(modStats, UCHAR_CONSTANT("imkubernetes")));
    STATSCOUNTER_INIT(ctrSubmitted, mutCtrSubmitted);
    STATSCOUNTER_INIT(ctrParseErrors, mutCtrParseErrors);
    STATSCOUNTER_INIT(ctrFilesDiscovered, mutCtrFilesDiscovered);
    STATSCOUNTER_INIT(ctrCriRecords, mutCtrCriRecords);
    STATSCOUNTER_INIT(ctrDockerJsonRecords, mutCtrDockerJsonRecords);
    STATSCOUNTER_INIT(ctrCacheHits, mutCtrCacheHits);
    STATSCOUNTER_INIT(ctrCacheMisses, mutCtrCacheMisses);
    STATSCOUNTER_INIT(ctrApiErrors, mutCtrApiErrors);
    STATSCOUNTER_INIT(ctrLostRatelimit, mutCtrLostRatelimit);
    CHKiRet(
        statsobj.AddCounter(modStats, UCHAR_CONSTANT("submitted"), ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrSubmitted));
    CHKiRet(statsobj.AddCounter(modStats, UCHAR_CONSTANT("parse.errors"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &ctrParseErrors));
    CHKiRet(statsobj.AddCounter(modStats, UCHAR_CONSTANT("files.discovered"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &ctrFilesDiscovered));
    CHKiRet(statsobj.AddCounter(modStats, UCHAR_CONSTANT("records.cri"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &ctrCriRecords));
    CHKiRet(statsobj.AddCounter(modStats, UCHAR_CONSTANT("records.dockerjson"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &ctrDockerJsonRecords));
    CHKiRet(statsobj.AddCounter(modStats, UCHAR_CONSTANT("kube.cache_hits"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &ctrCacheHits));
    CHKiRet(statsobj.AddCounter(modStats, UCHAR_CONSTANT("kube.cache_misses"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &ctrCacheMisses));
    CHKiRet(statsobj.AddCounter(modStats, UCHAR_CONSTANT("kube.api_errors"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &ctrApiErrors));
    CHKiRet(statsobj.AddCounter(modStats, UCHAR_CONSTANT("ratelimit.discarded"), ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &ctrLostRatelimit));
    CHKiRet(statsobj.ConstructFinalize(modStats));
finalize_it:
ENDactivateCnf

BEGINfreeCnf
    CODESTARTfreeCnf;
    free(loadModConf->logFileGlob);
    free(loadModConf->kubernetesUrl);
    free(loadModConf->token);
    free(loadModConf->tokenFile);
    free(loadModConf->caCertFile);
    if (modStats != NULL) {
        statsobj.Destruct(&modStats);
        modStats = NULL;
    }
ENDfreeCnf

BEGINrunInput
    CODESTARTrunInput;
    CHKiRet(ratelimitNew(&ratelimiter, "imkubernetes", NULL));
    CHKiRet(initApiClient());
    while (glbl.GetGlobalInputTermState() == 0) {
        scanAndProcessFiles();
        srSleep(runModConf->iPollInterval, 0);
    }
finalize_it:
    freeApiClient();
    freePodCache();
    freeFileStates();
    free(gLineBuf);
    gLineBuf = NULL;
    gLineBufCap = 0;
    if (ratelimiter != NULL) {
        ratelimitDestruct(ratelimiter);
        ratelimiter = NULL;
    }
ENDrunInput

BEGINwillRun
    CODESTARTwillRun;
ENDwillRun

BEGINafterRun
    CODESTARTafterRun;
ENDafterRun

BEGINmodExit
    CODESTARTmodExit;
    if (pInputName != NULL) {
        prop.Destruct(&pInputName);
    }
    objRelease(parser, CORE_COMPONENT);
    objRelease(glbl, CORE_COMPONENT);
    objRelease(prop, CORE_COMPONENT);
    objRelease(statsobj, CORE_COMPONENT);
    objRelease(datetime, CORE_COMPONENT);
ENDmodExit

BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURENonCancelInputTermination) {
        iRet = RS_RET_OK;
    }
ENDisCompatibleWithFeature

BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_IMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
    CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES;
    CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES;
ENDqueryEtryPt

BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION;
    CODEmodInit_QueryRegCFSLineHdlr

        CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(prop, CORE_COMPONENT));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));
    CHKiRet(objUse(datetime, CORE_COMPONENT));
    CHKiRet(objUse(parser, CORE_COMPONENT));

    CHKiRet(prop.Construct(&pInputName));
    CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imkubernetes"), sizeof("imkubernetes") - 1));
    CHKiRet(prop.ConstructFinalize(pInputName));
ENDmodInit
