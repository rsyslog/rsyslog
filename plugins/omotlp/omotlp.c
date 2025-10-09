/**
 * @file omotlp.c
 * @brief OpenTelemetry (OTLP) output module for rsyslog
 *
 * This module provides native OpenTelemetry log export capabilities using
 * the OTLP/HTTP JSON protocol. It supports batching, compression, retry
 * semantics, TLS/mTLS, and proxy configuration.
 *
 * Concurrency & Locking:
 * - Shared configuration lives in the per-action instanceData structure and is
 *   read-only after instantiation.
 * - Each worker maintains its own HTTP client and batching buffer guarded by a
 *   mutex so no cross-worker locks are required.
 * - A per-worker flush thread wakes periodically to service batch timeouts and
 *   returns control to the rsyslog action queue for retry/backoff decisions.
 *
 * Copyright 2025 Adiscon GmbH.
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
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#include "conf.h"
#include "datetime.h"
#include "msg.h"
#include "srUtils.h"
#include "stringbuf.h"
#include "syslogd-types.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "statsobj.h"

#include "otlp_json.h"
#include "omotlp_http.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("omotlp")

DEF_OMOD_STATIC_DATA;
DEFobjCurrIf(datetime);
DEFobjCurrIf(prop);
DEFobjCurrIf(statsobj);

typedef enum omotlp_compression_e {
    OMOTLP_COMPRESSION_UNSET = 0,
    OMOTLP_COMPRESSION_NONE,
    OMOTLP_COMPRESSION_GZIP,
} omotlp_compression_t;

typedef struct header_list_s {
    char **values;
    size_t count;
    size_t capacity;
} header_list_t;

typedef struct attribute_map_entry_s {
    char *rsyslog_property; /* Source property name */
    char *otlp_attribute; /* Target OTLP attribute name */
} attribute_map_entry_t;

typedef struct attribute_map_s {
    attribute_map_entry_t *entries;
    size_t count;
    size_t capacity;
} attribute_map_t;

typedef struct severity_map_entry_s {
    int syslog_priority; /* 0-7 */
    uint32_t severity_number; /* OTLP severity number */
    char *severity_text; /* OTLP severity text */
} severity_map_entry_t;

typedef struct severity_map_s {
    severity_map_entry_t entries[8]; /* One per syslog priority */
    int configured; /* 1 if custom mapping, 0 if default */
} severity_map_t;

enum {
    OMOTLP_OMSR_IDX_MESSAGE = 0,
    OMOTLP_OMSR_IDX_BODY = 1,
};

typedef struct _instanceData {
    uchar *endpoint;
    uchar *path;
    uchar *protocol;
    uchar *bodyTemplateName;
    uchar *url;
    long requestTimeoutMs;
    size_t batchMaxItems;
    size_t batchMaxBytes;
    long batchTimeoutMs;
    long retryInitialMs;
    long retryMaxMs;
    unsigned int retryMaxRetries;
    unsigned int retryJitterPercent;
    omotlp_compression_t compression_mode;
    int compressionConfigured;
    int headersConfigured;
    int bearerConfigured;
    int timeoutConfigured;
    header_list_t headers;
    uchar *resourceServiceInstanceId;
    uchar *resourceDeploymentEnvironment;
    /* Full resource configuration */
    uchar *resourceJson; /* JSON string with resource attributes */
    struct json_object *resourceJsonParsed; /* Parsed JSON object (cached) */
    /* Trace correlation property names */
    uchar *traceIdPropertyName;
    uchar *spanIdPropertyName;
    uchar *traceFlagsPropertyName;
    /* Custom attribute mapping */
    attribute_map_t attributeMap;
    /* Custom severity mapping */
    severity_map_t severityMap;
    /* TLS configuration */
    uchar *tlsCaCertFile;
    uchar *tlsCaCertDir;
    uchar *tlsClientCertFile;
    uchar *tlsClientKeyFile;
    int tlsVerifyHostname; /* 0 = disabled, 1 = enabled */
    int tlsVerifyPeer; /* 0 = disabled, 1 = enabled */
    /* Proxy configuration */
    uchar *proxyUrl;
    uchar *proxyUser;
    uchar *proxyPassword;
} instanceData;

typedef struct omotlp_batch_entry_s {
    omotlp_log_record_t record;
    char *body;
    char *hostname;
    char *app_name;
    char *proc_id;
    char *msg_id;
    char *trace_id; /* Allocated string for trace_id */
    char *span_id; /* Allocated string for span_id */
} omotlp_batch_entry_t;

typedef struct omotlp_batch_state_s {
    omotlp_batch_entry_t *entries;
    size_t count;
    size_t capacity;
    size_t estimated_bytes;
    long long first_enqueue_ms;
} omotlp_batch_state_t;

typedef struct wrkrInstanceData {
    instanceData *pData;
    omotlp_http_client_t *http_client;
    omotlp_batch_state_t batch;
    pthread_t flush_thread;
    pthread_mutex_t batch_mutex;
    int flush_thread_running;
    int flush_thread_stop;

    /* Statistics counters */
    statsobj_t *stats;
    STATSCOUNTER_DEF(ctrBatchesSubmitted, mutCtrBatchesSubmitted);
    STATSCOUNTER_DEF(ctrBatchesSuccess, mutCtrBatchesSuccess);
    STATSCOUNTER_DEF(ctrBatchesRetried, mutCtrBatchesRetried);
    STATSCOUNTER_DEF(ctrBatchesDropped, mutCtrBatchesDropped);
    STATSCOUNTER_DEF(ctrHttpStatus4xx, mutCtrHttpStatus4xx);
    STATSCOUNTER_DEF(ctrHttpStatus5xx, mutCtrHttpStatus5xx);
    STATSCOUNTER_DEF(ctrRecordsSent, mutCtrRecordsSent);
    STATSCOUNTER_DEF(httpRequestLatencyMs, mutHttpRequestLatencyMs);
} wrkrInstanceData_t;

struct modConfData_s {
    rsconf_t *pConf;
};

static modConfData_t *loadModConf = NULL;
static modConfData_t *runModConf = NULL;

static struct cnfparamdescr actpdescr[] = {{"endpoint", eCmdHdlrString, 0},
                                           {"path", eCmdHdlrString, 0},
                                           {"protocol", eCmdHdlrGetWord, 0},
                                           {"template", eCmdHdlrGetWord, 0},
                                           {"timeout.ms", eCmdHdlrGetWord, 0},
                                           {"compression", eCmdHdlrGetWord, 0},
                                           {"batch.max_items", eCmdHdlrGetWord, 0},
                                           {"batch.max_bytes", eCmdHdlrGetWord, 0},
                                           {"batch.timeout.ms", eCmdHdlrGetWord, 0},
                                           {"retry.initial.ms", eCmdHdlrGetWord, 0},
                                           {"retry.max.ms", eCmdHdlrGetWord, 0},
                                           {"retry.max_retries", eCmdHdlrGetWord, 0},
                                           {"retry.jitter.percent", eCmdHdlrGetWord, 0},
                                           {"headers", eCmdHdlrString, 0},
                                           {"bearer_token", eCmdHdlrString, 0},
                                           {"resource.service_instance_id", eCmdHdlrString, 0},
                                           {"resource.deployment.environment", eCmdHdlrString, 0},
                                           {"resource", eCmdHdlrString, 0}, /* Full JSON resource configuration */
                                           {"trace_id.property", eCmdHdlrString, 0},
                                           {"span_id.property", eCmdHdlrString, 0},
                                           {"trace_flags.property", eCmdHdlrString, 0},
                                           {"attributeMap", eCmdHdlrString, 0},
                                           {"severity.map", eCmdHdlrString, 0},
                                           {"tls.cacert", eCmdHdlrString, 0},
                                           {"tls.cadir", eCmdHdlrString, 0},
                                           {"tls.cert", eCmdHdlrString, 0},
                                           {"tls.key", eCmdHdlrString, 0},
                                           {"tls.verify_hostname", eCmdHdlrGetWord, 0},
                                           {"tls.verify_peer", eCmdHdlrGetWord, 0},
                                           {"proxy", eCmdHdlrString, 0},
                                           {"proxy.user", eCmdHdlrString, 0},
                                           {"proxy.password", eCmdHdlrString, 0}};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

static rsRetVal parse_headers_env(instanceData *pData, const char *text);
static rsRetVal parse_timeout_string(const char *text, long *out);
static rsRetVal set_compression_mode(instanceData *pData, const char *value);

static void lowercaseInPlace(uchar *value) {
    char *cursor;

    if (value == NULL) {
        return;
    }

    for (cursor = (char *)value; *cursor != '\0'; ++cursor) {
        *cursor = (char)tolower((unsigned char)*cursor);
    }
}

static rsRetVal assignParamFromCStr(uchar **target, const char *value) {
    uchar *tmp;
    DEFiRet;

    if (value == NULL) {
        goto finalize_it;
    }

    tmp = (uchar *)strdup(value);
    if (tmp == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    free(*target);
    *target = tmp;

finalize_it:
    RETiRet;
}

static const char *firstPopulatedEnv(const char *const *names) {
    size_t i;

    if (names == NULL) {
        return NULL;
    }

    for (i = 0; names[i] != NULL; ++i) {
        const char *value = getenv(names[i]);
        if (value != NULL && value[0] != '\0') {
            return value;
        }
    }

    return NULL;
}

static rsRetVal applyEnvDefaults(instanceData *pData) {
    const char *value;
    static const char *const endpointEnvVars[] = {"OTEL_EXPORTER_OTLP_LOGS_ENDPOINT", "OTEL_EXPORTER_OTLP_ENDPOINT",
                                                  NULL};
    static const char *const protocolEnvVars[] = {"OTEL_EXPORTER_OTLP_LOGS_PROTOCOL", "OTEL_EXPORTER_OTLP_PROTOCOL",
                                                  NULL};
    static const char *const timeoutEnvVars[] = {"OTEL_EXPORTER_OTLP_LOGS_TIMEOUT", "OTEL_EXPORTER_OTLP_TIMEOUT", NULL};
    static const char *const compressionEnvVars[] = {"OTEL_EXPORTER_OTLP_LOGS_COMPRESSION",
                                                     "OTEL_EXPORTER_OTLP_COMPRESSION", NULL};
    static const char *const headersEnvVars[] = {"OTEL_EXPORTER_OTLP_LOGS_HEADERS", "OTEL_EXPORTER_OTLP_HEADERS", NULL};

    DEFiRet;

    if (pData->endpoint == NULL) {
        value = firstPopulatedEnv(endpointEnvVars);
        if (value != NULL) {
            CHKiRet(assignParamFromCStr(&pData->endpoint, value));
        }
    }

    if (pData->protocol == NULL) {
        value = firstPopulatedEnv(protocolEnvVars);
        if (value != NULL) {
            CHKiRet(assignParamFromCStr(&pData->protocol, value));
        }
    }

    if (!pData->timeoutConfigured) {
        value = firstPopulatedEnv(timeoutEnvVars);
        if (value != NULL) {
            long timeout_ms;
            CHKiRet(parse_timeout_string(value, &timeout_ms));
            pData->requestTimeoutMs = timeout_ms;
        }
    }

    if (!pData->compressionConfigured && pData->compression_mode == OMOTLP_COMPRESSION_UNSET) {
        value = firstPopulatedEnv(compressionEnvVars);
        if (value != NULL) {
            CHKiRet(set_compression_mode(pData, value));
        }
    }

    if (!pData->headersConfigured) {
        value = firstPopulatedEnv(headersEnvVars);
        if (value != NULL) {
            CHKiRet(parse_headers_env(pData, value));
        }
    }

finalize_it:
    RETiRet;
}

/**
 * @brief Split endpoint URL into base URL and path components
 *
 * If the endpoint contains a path component (e.g., "http://host:4318/v1/logs"),
 * this function splits it into the base URL ("http://host:4318") and path
 * ("/v1/logs"). This allows users to specify the full URL in the endpoint
 * parameter while still supporting separate path configuration.
 *
 * @param[in,out] pData Instance data containing endpoint and path
 * @return RS_RET_OK on success or if no split is needed
 */
static rsRetVal ensureEndpointPathSplit(instanceData *pData) {
    char *endpoint;
    char *schemeSeparator;
    char *pathStart;
    char *baseDup = NULL;
    char *pathDup = NULL;
    size_t baseLength;

    DEFiRet;

    if (pData->endpoint == NULL || pData->path != NULL) {
        goto finalize_it;
    }

    endpoint = (char *)pData->endpoint;
    schemeSeparator = strstr(endpoint, "://");
    if (schemeSeparator != NULL) {
        pathStart = strchr(schemeSeparator + 3, '/');
    } else {
        pathStart = strchr(endpoint, '/');
    }

    if (pathStart == NULL || *pathStart == '\0' || pathStart == endpoint) {
        goto finalize_it;
    }

    baseLength = (size_t)(pathStart - endpoint);
    baseDup = (char *)malloc(baseLength + 1);
    if (baseDup == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    memcpy(baseDup, endpoint, baseLength);
    baseDup[baseLength] = '\0';

    pathDup = strdup(pathStart);
    if (pathDup == NULL) {
        free(baseDup);
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    free(pData->endpoint);
    pData->endpoint = (uchar *)baseDup;
    pData->path = (uchar *)pathDup;

finalize_it:
    RETiRet;
}

static rsRetVal validateProtocol(instanceData *pData) {
    DEFiRet;

    if (pData->protocol == NULL) {
        goto finalize_it;
    }

    if (!strcmp((char *)pData->protocol, "http/json")) {
        goto finalize_it;
    }

    LogError(0, RS_RET_NOT_IMPLEMENTED, "omotlp: protocol '%s' is not supported by the scaffolding build",
             pData->protocol);
    ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);

finalize_it:
    RETiRet;
}

/**
 * @brief Build the effective URL from endpoint and path components
 *
 * Combines the endpoint base URL and path into a single effective URL,
 * handling edge cases like duplicate slashes or missing slashes between
 * components. The result is stored in pData->url.
 *
 * @param[in,out] pData Instance data with endpoint and path to combine
 * @return RS_RET_OK on success, RS_RET_PARAM_ERROR if endpoint is NULL,
 *         RS_RET_OUT_OF_MEMORY on allocation failure
 */
static rsRetVal buildEffectiveUrl(instanceData *pData) {
    const char *endpoint = (const char *)pData->endpoint;
    const char *path = (const char *)pData->path;
    size_t endpoint_len;
    size_t path_len;
    size_t total;
    int endpoint_has_slash;
    int path_has_slash;
    char *buffer = NULL;
    size_t cursor = 0;

    DEFiRet;

    if (endpoint == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    endpoint_len = strlen(endpoint);
    path_len = (path != NULL) ? strlen(path) : 0u;
    endpoint_has_slash = (endpoint_len > 0 && endpoint[endpoint_len - 1] == '/');
    path_has_slash = (path_len > 0 && path[0] == '/');

    total = endpoint_len + path_len + 2u;
    buffer = malloc(total);
    if (buffer == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    memcpy(buffer, endpoint, endpoint_len);
    cursor = endpoint_len;

    if (path_len > 0) {
        if (!endpoint_has_slash && !path_has_slash) {
            buffer[cursor++] = '/';
        } else if (endpoint_has_slash && path_has_slash) {
            ++path;
            --path_len;
        }

        memcpy(buffer + cursor, path, path_len);
        cursor += path_len;
    }

    buffer[cursor] = '\0';

    free(pData->url);
    pData->url = (uchar *)buffer;

finalize_it:
    if (iRet != RS_RET_OK && buffer != NULL) {
        free(buffer);
    }
    RETiRet;
}

typedef struct severity_mapping_s {
    uint32_t number;
    const char *text;
} severity_mapping_t;

static const severity_mapping_t severity_lookup[8] = {{24u, "EMERGENCY"}, {23u, "ALERT"},   {22u, "CRITICAL"},
                                                      {17u, "ERROR"},     {13u, "WARNING"}, {11u, "NOTICE"},
                                                      {9u, "INFO"},       {5u, "DEBUG"}};

#define OMOTLP_DEFAULT_BATCH_MAX_ITEMS 512u
#define OMOTLP_DEFAULT_BATCH_MAX_BYTES (512u * 1024u)
#define OMOTLP_DEFAULT_BATCH_TIMEOUT_MS 5000L
#define OMOTLP_DEFAULT_RETRY_INITIAL_MS 1000L
#define OMOTLP_DEFAULT_RETRY_MAX_MS 30000L
#define OMOTLP_DEFAULT_RETRY_MAX_RETRIES 5u
#define OMOTLP_DEFAULT_RETRY_JITTER_PERCENT 20u

#define OMOTLP_BATCH_BASE_OVERHEAD 256u
#define OMOTLP_BATCH_RECORD_OVERHEAD 256u
#define OMOTLP_IDLE_FLUSH_INTERVAL_MS 1000L

static void header_list_init(header_list_t *list) {
    if (list == NULL) {
        return;
    }

    list->values = NULL;
    list->count = 0u;
    list->capacity = 0u;
}

static void header_list_clear(header_list_t *list) {
    size_t i;

    if (list == NULL) {
        return;
    }

    for (i = 0; i < list->count; ++i) {
        free(list->values[i]);
        list->values[i] = NULL;
    }

    list->count = 0u;
}

static void header_list_destroy(header_list_t *list) {
    if (list == NULL) {
        return;
    }

    header_list_clear(list);
    free(list->values);
    list->values = NULL;
    list->capacity = 0u;
}

static rsRetVal header_list_add(header_list_t *list, const char *header) {
    char **tmp;
    char *dup = NULL;
    size_t new_capacity;

    DEFiRet;

    if (list == NULL || header == NULL || header[0] == '\0') {
        goto finalize_it;
    }

    if (list->count == list->capacity) {
        new_capacity = (list->capacity == 0u) ? 4u : list->capacity * 2u;
        tmp = (char **)realloc(list->values, new_capacity * sizeof(char *));
        if (tmp == NULL) {
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        list->values = tmp;
        list->capacity = new_capacity;
    }

    dup = strdup(header);
    if (dup == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    list->values[list->count++] = dup;
    dup = NULL;

finalize_it:
    if (dup != NULL) {
        free(dup);
    }
    RETiRet;
}

static rsRetVal header_list_add_kv(header_list_t *list, const char *key, const char *value) {
    char *buffer = NULL;
    size_t key_len;
    size_t value_len;
    DEFiRet;

    if (key == NULL || key[0] == '\0') {
        goto finalize_it;
    }

    key_len = strlen(key);
    value_len = (value != NULL) ? strlen(value) : 0u;
    buffer = (char *)malloc(key_len + 2u + value_len + 1u);
    if (buffer == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    memcpy(buffer, key, key_len);
    buffer[key_len] = ':';
    buffer[key_len + 1u] = ' ';
    if (value_len > 0u) {
        memcpy(buffer + key_len + 2u, value, value_len);
    }
    buffer[key_len + 2u + value_len] = '\0';

    CHKiRet(header_list_add(list, buffer));

finalize_it:
    if (buffer != NULL) {
        free(buffer);
    }
    RETiRet;
}

static void attribute_map_init(attribute_map_t *map) {
    if (map == NULL) {
        return;
    }
    map->entries = NULL;
    map->count = 0u;
    map->capacity = 0u;
}

static void attribute_map_clear(attribute_map_t *map) {
    size_t i;

    if (map == NULL) {
        return;
    }

    for (i = 0; i < map->count; ++i) {
        free(map->entries[i].rsyslog_property);
        free(map->entries[i].otlp_attribute);
        map->entries[i].rsyslog_property = NULL;
        map->entries[i].otlp_attribute = NULL;
    }

    map->count = 0u;
}

static void attribute_map_destroy(attribute_map_t *map) {
    if (map == NULL) {
        return;
    }

    attribute_map_clear(map);
    free(map->entries);
    map->entries = NULL;
    map->capacity = 0u;
}

static rsRetVal attribute_map_add(attribute_map_t *map, const char *rsyslog_prop, const char *otlp_attr) {
    attribute_map_entry_t *tmp;
    size_t new_capacity;
    char *prop_dup = NULL;
    char *attr_dup = NULL;

    DEFiRet;

    if (map == NULL || rsyslog_prop == NULL || otlp_attr == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if (map->count == map->capacity) {
        new_capacity = (map->capacity == 0u) ? 4u : map->capacity * 2u;
        tmp = (attribute_map_entry_t *)realloc(map->entries, new_capacity * sizeof(attribute_map_entry_t));
        if (tmp == NULL) {
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        memset(tmp + map->count, 0, (new_capacity - map->count) * sizeof(attribute_map_entry_t));
        map->entries = tmp;
        map->capacity = new_capacity;
    }

    prop_dup = strdup(rsyslog_prop);
    attr_dup = strdup(otlp_attr);

    if (prop_dup == NULL || attr_dup == NULL) {
        free(prop_dup);
        free(attr_dup);
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    map->entries[map->count].rsyslog_property = prop_dup;
    map->entries[map->count].otlp_attribute = attr_dup;
    ++map->count;

finalize_it:
    RETiRet;
}


static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

static void percent_decode(char *value) {
    char *src;
    char *dst;

    if (value == NULL) {
        return;
    }

    src = value;
    dst = value;
    while (*src != '\0') {
        if (*src == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            int hi = hex_value(src[1]);
            int lo = hex_value(src[2]);
            if (hi >= 0 && lo >= 0) {
                *dst++ = (char)((hi << 4) | lo);
                src += 3;
                continue;
            }
        }

        *dst++ = *src++;
    }

    *dst = '\0';
}

static char *trim_whitespace(char *value) {
    char *start;
    char *end;

    if (value == NULL) {
        return NULL;
    }

    start = value;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        ++start;
    }

    end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) {
        --end;
    }

    *end = '\0';
    return start;
}

static rsRetVal parse_long_param(const char *name, es_str_t *value, long min, long *out) {
    char *text = NULL;
    char *end = NULL;
    long parsed;

    DEFiRet;

    if (value == NULL || out == NULL) {
        goto finalize_it;
    }

    text = (char *)es_str2cstr(value, NULL);
    if (text == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: invalid numeric value '%s' for parameter %s", text, name);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if (parsed < min) {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: value %ld for %s is below the minimum %ld", parsed, name, min);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    *out = parsed;

finalize_it:
    if (text != NULL) {
        free(text);
    }
    RETiRet;
}

static rsRetVal parse_size_param(const char *name, es_str_t *value, size_t min, size_t *out) {
    char *text = NULL;
    char *end = NULL;
    unsigned long long parsed;

    DEFiRet;

    if (value == NULL || out == NULL) {
        goto finalize_it;
    }

    text = (char *)es_str2cstr(value, NULL);
    if (text == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: invalid size value '%s' for parameter %s", text, name);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if (parsed < (unsigned long long)min) {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: value %s for %s is below the minimum %zu", text, name, min);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    *out = (size_t)parsed;

finalize_it:
    if (text != NULL) {
        free(text);
    }
    RETiRet;
}

static rsRetVal parse_uint_param(const char *name, es_str_t *value, unsigned int min, unsigned int *out) {
    char *text = NULL;
    char *end = NULL;
    unsigned long parsed;

    DEFiRet;

    if (value == NULL || out == NULL) {
        goto finalize_it;
    }

    text = (char *)es_str2cstr(value, NULL);
    if (text == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: invalid numeric value '%s' for parameter %s", text, name);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if (parsed < (unsigned long)min) {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: value %lu for %s is below the minimum %u", parsed, name, min);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    *out = (unsigned int)parsed;

finalize_it:
    if (text != NULL) {
        free(text);
    }
    RETiRet;
}

static rsRetVal parse_headers_json(instanceData *pData, const char *text) {
    struct json_object *root = NULL;
    struct json_object_iterator iter;
    struct json_object_iterator iter_end;

    DEFiRet;

    if (text == NULL || text[0] == '\0') {
        goto finalize_it;
    }

    root = fjson_tokener_parse(text);
    if (root == NULL) {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: failed to parse headers JSON '%s'", text);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if (!fjson_object_is_type(root, fjson_type_object)) {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: headers parameter must be a JSON object");
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    iter = json_object_iter_begin(root);
    iter_end = json_object_iter_end(root);
    while (!json_object_iter_equal(&iter, &iter_end)) {
        const char *key = json_object_iter_peek_name(&iter);
        struct json_object *value_obj = json_object_iter_peek_value(&iter);
        const char *value = NULL;

        if (value_obj != NULL) {
            if (!fjson_object_is_type(value_obj, fjson_type_string)) {
                LogError(0, RS_RET_PARAM_ERROR, "omotlp: header '%s' value must be a string", key);
                ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            }
            value = fjson_object_get_string(value_obj);
        }

        CHKiRet(header_list_add_kv(&pData->headers, key, value));
        json_object_iter_next(&iter);
    }

finalize_it:
    if (root != NULL) {
        fjson_object_put(root);
    }
    RETiRet;
}

static rsRetVal parse_attribute_map(instanceData *pData, const char *json_text) {
    struct json_object *root = NULL;
    struct json_object_iterator iter;
    struct json_object_iterator iter_end;

    DEFiRet;

    if (json_text == NULL || json_text[0] == '\0') {
        goto finalize_it;
    }

    root = fjson_tokener_parse(json_text);
    if (root == NULL) {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: failed to parse attributeMap JSON: %s", json_text);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if (!fjson_object_is_type(root, fjson_type_object)) {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: attributeMap must be a JSON object");
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    iter = json_object_iter_begin(root);
    iter_end = json_object_iter_end(root);

    while (!json_object_iter_equal(&iter, &iter_end)) {
        const char *rsyslog_prop = json_object_iter_peek_name(&iter);
        struct json_object *value_obj = json_object_iter_peek_value(&iter);
        const char *otlp_attr = NULL;

        if (value_obj == NULL || !fjson_object_is_type(value_obj, fjson_type_string)) {
            LogError(0, RS_RET_PARAM_ERROR, "omotlp: attributeMap value for '%s' must be a string", rsyslog_prop);
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }

        otlp_attr = fjson_object_get_string(value_obj);

        /* Add to attribute map */
        CHKiRet(attribute_map_add(&pData->attributeMap, rsyslog_prop, otlp_attr));

        json_object_iter_next(&iter);
    }

finalize_it:
    if (root != NULL) {
        fjson_object_put(root);
    }
    RETiRet;
}

static rsRetVal parse_severity_map(instanceData *pData, const char *json_text) {
    struct json_object *root = NULL;
    struct json_object_iterator iter;
    struct json_object_iterator iter_end;
    int priority;
    uint32_t severity_number;
    const char *severity_text = NULL;
    char *severity_text_dup = NULL;

    DEFiRet;

    if (json_text == NULL || json_text[0] == '\0') {
        goto finalize_it;
    }

    root = fjson_tokener_parse(json_text);
    if (root == NULL) {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: failed to parse severity.map JSON: %s", json_text);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if (!fjson_object_is_type(root, fjson_type_object)) {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: severity.map must be a JSON object");
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    /* Initialize all entries */
    for (priority = 0; priority < 8; ++priority) {
        pData->severityMap.entries[priority].syslog_priority = priority;
        pData->severityMap.entries[priority].severity_number = 0u;
        pData->severityMap.entries[priority].severity_text = NULL;
    }

    iter = json_object_iter_begin(root);
    iter_end = json_object_iter_end(root);

    while (!json_object_iter_equal(&iter, &iter_end)) {
        const char *key = json_object_iter_peek_name(&iter);
        struct json_object *value_obj = json_object_iter_peek_value(&iter);
        struct json_object *number_obj = NULL;
        struct json_object *text_obj = NULL;

        errno = 0;
        priority = (int)strtol(key, NULL, 10);
        if (errno != 0 || priority < 0 || priority > 7) {
            LogError(0, RS_RET_PARAM_ERROR, "omotlp: severity.map key '%s' must be a number 0-7", key);
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }

        if (value_obj == NULL || !fjson_object_is_type(value_obj, fjson_type_object)) {
            LogError(0, RS_RET_PARAM_ERROR, "omotlp: severity.map value for priority %d must be an object", priority);
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }

        /* Use fjson_object_object_get_ex for efficient key lookup */
        fjson_object_object_get_ex(value_obj, "number", &number_obj);
        fjson_object_object_get_ex(value_obj, "text", &text_obj);

        if (number_obj == NULL || !fjson_object_is_type(number_obj, fjson_type_int)) {
            LogError(0, RS_RET_PARAM_ERROR, "omotlp: severity.map[%d] must have 'number' field (integer)", priority);
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }

        if (text_obj == NULL || !fjson_object_is_type(text_obj, fjson_type_string)) {
            LogError(0, RS_RET_PARAM_ERROR, "omotlp: severity.map[%d] must have 'text' field (string)", priority);
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }

        severity_number = (uint32_t)fjson_object_get_int64(number_obj);
        severity_text = fjson_object_get_string(text_obj);

        severity_text_dup = strdup(severity_text);
        if (severity_text_dup == NULL) {
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }

        pData->severityMap.entries[priority].severity_number = severity_number;
        pData->severityMap.entries[priority].severity_text = severity_text_dup;
        severity_text_dup = NULL; /* Ownership transferred */

        json_object_iter_next(&iter);
    }

    pData->severityMap.configured = 1;

finalize_it:
    if (root != NULL) {
        fjson_object_put(root);
    }
    if (severity_text_dup != NULL) {
        free(severity_text_dup);
    }
    RETiRet;
}

static rsRetVal parse_headers_env(instanceData *pData, const char *text) {
    char *dup = NULL;
    char *cursor;

    DEFiRet;

    if (text == NULL || text[0] == '\0') {
        goto finalize_it;
    }

    dup = strdup(text);
    if (dup == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    cursor = dup;
    while (cursor != NULL && *cursor != '\0') {
        char *token = cursor;
        char *next = strchr(cursor, ',');
        char *key;
        char *value;

        if (next != NULL) {
            *next = '\0';
            cursor = next + 1;
        } else {
            cursor = NULL;
        }

        token = trim_whitespace(token);
        if (token == NULL || token[0] == '\0') {
            continue;
        }

        value = strchr(token, '=');
        if (value == NULL) {
            LogError(0, RS_RET_PARAM_ERROR, "omotlp: header entry '%s' is missing '='", token);
            ABORT_FINALIZE(RS_RET_PARAM_ERROR);
        }

        *value = '\0';
        ++value;

        key = trim_whitespace(token);
        value = trim_whitespace(value);

        percent_decode(key);
        percent_decode(value);

        CHKiRet(header_list_add_kv(&pData->headers, key, value));
    }

finalize_it:
    if (dup != NULL) {
        free(dup);
    }
    RETiRet;
}

static rsRetVal parse_timeout_string(const char *text, long *out) {
    char *dup = NULL;
    char *number;
    char *end = NULL;
    long multiplier = 1;
    long parsed;
    size_t len;

    DEFiRet;

    if (text == NULL || out == NULL) {
        goto finalize_it;
    }

    dup = strdup(text);
    if (dup == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    len = strlen(dup);
    if (len >= 2 && dup[len - 2] == 'm' && dup[len - 1] == 's') {
        dup[len - 2] = '\0';
    } else if (len >= 1 && dup[len - 1] == 's') {
        dup[len - 1] = '\0';
        multiplier = 1000;
    }

    number = trim_whitespace(dup);
    errno = 0;
    parsed = strtol(number, &end, 10);
    if (errno != 0 || end == number || *end != '\0') {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: invalid timeout value '%s'", text);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if (parsed < 0) {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: timeout must be non-negative, got %ld", parsed);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if (multiplier != 1 && parsed > LONG_MAX / multiplier) {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: timeout '%s' exceeds range", text);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    *out = parsed * multiplier;

finalize_it:
    if (dup != NULL) {
        free(dup);
    }
    RETiRet;
}

static rsRetVal set_compression_mode(instanceData *pData, const char *value) {
    char buffer[16];
    size_t len;
    size_t i;

    DEFiRet;

    if (pData == NULL || value == NULL || value[0] == '\0') {
        goto finalize_it;
    }

    len = strlen(value);
    if (len >= sizeof(buffer)) {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: unsupported compression value '%s'", value);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    for (i = 0; i < len; ++i) {
        buffer[i] = (char)tolower((unsigned char)value[i]);
    }
    buffer[len] = '\0';

    if (!strcmp(buffer, "gzip")) {
        pData->compression_mode = OMOTLP_COMPRESSION_GZIP;
    } else if (!strcmp(buffer, "none")) {
        pData->compression_mode = OMOTLP_COMPRESSION_NONE;
    } else {
        LogError(0, RS_RET_PARAM_ERROR, "omotlp: compression '%s' is not supported", value);
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

finalize_it:
    RETiRet;
}

static rsRetVal duplicate_optional_string(const char *source, char **dest) {
    char *dup = NULL;

    DEFiRet;

    if (dest == NULL) {
        goto finalize_it;
    }

    *dest = NULL;

    if (source == NULL || source[0] == '\0') {
        goto finalize_it;
    }

    dup = strdup(source);
    if (dup == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    *dest = dup;
    dup = NULL;

finalize_it:
    if (dup != NULL) {
        free(dup);
    }
    RETiRet;
}

static void mapSeverity(int syslogSeverity, instanceData *pData, omotlp_log_record_t *record) {
    if (syslogSeverity < 0 || syslogSeverity > 7) {
        record->severity_number = 0u;
        record->severity_text = NULL;
        return;
    }

    if (pData != NULL && pData->severityMap.configured &&
        pData->severityMap.entries[syslogSeverity].severity_text != NULL) {
        /* Use custom mapping if it exists for this priority */
        record->severity_number = pData->severityMap.entries[syslogSeverity].severity_number;
        record->severity_text = pData->severityMap.entries[syslogSeverity].severity_text;
    } else {
        /* Use default mapping (either no custom map configured, or this priority not mapped) */
        record->severity_number = severity_lookup[syslogSeverity].number;
        record->severity_text = severity_lookup[syslogSeverity].text;
    }
}

static uint64_t scaleFractionToNanos(int fraction, int precision) {
    uint64_t value;

    if (precision <= 0 || fraction <= 0) {
        return 0u;
    }

    value = (uint64_t)fraction;
    if (precision > 9) {
        int diff = precision - 9;
        while (diff-- > 0 && value > 0u) {
            value /= 10u;
        }
    } else if (precision < 9) {
        int diff = 9 - precision;
        while (diff-- > 0) {
            value *= 10u;
        }
    }

    return value;
}

static uint64_t syslogTimeToUnixNanos(const struct syslogTime *timestamp) {
    if (timestamp == NULL) {
        return 0u;
    }

    return ((uint64_t)datetime.syslogTime2time_t(timestamp) * 1000000000ull) +
           scaleFractionToNanos(timestamp->secfrac, timestamp->secfracPrecision);
}

static const char *cstrToConst(cstr_t *value) {
    return value == NULL ? NULL : (const char *)rsCStrGetSzStrNoNULL(value);
}

static const char *extractAppName(const smsg_t *msg) {
    const char *candidate;

    if (msg == NULL) {
        return NULL;
    }

    candidate = cstrToConst(msg->pCSAPPNAME);
    if (candidate != NULL && candidate[0] != '\0') {
        return candidate;
    }

    if (msg->iLenPROGNAME > 0 && msg->PROGNAME.ptr != NULL) {
        return (const char *)msg->PROGNAME.ptr;
    }

    return NULL;
}

static const char *extractProcId(const smsg_t *msg) {
    const char *candidate;

    if (msg == NULL) {
        return NULL;
    }

    candidate = cstrToConst(msg->pCSPROCID);
    if (candidate != NULL && candidate[0] != '\0') {
        return candidate;
    }

    return NULL;
}

static const char *extractMsgId(const smsg_t *msg) {
    const char *candidate;

    if (msg == NULL) {
        return NULL;
    }

    candidate = cstrToConst(msg->pCSMSGID);
    if (candidate != NULL && candidate[0] != '\0') {
        return candidate;
    }

    return NULL;
}

/**
 * Validates a hex string for trace_id (32 hex characters = 128 bits)
 * Returns 1 if valid, 0 otherwise
 */
static int is_valid_trace_id(const char *value) {
    size_t len;
    size_t i;

    if (value == NULL) {
        return 0;
    }

    len = strlen(value);
    if (len != 32) {
        return 0;
    }

    for (i = 0; i < len; ++i) {
        if (!isxdigit((unsigned char)value[i])) {
            return 0;
        }
    }

    return 1;
}

/**
 * Validates a hex string for span_id (16 hex characters = 64 bits)
 * Returns 1 if valid, 0 otherwise
 */
static int is_valid_span_id(const char *value) {
    size_t len;
    size_t i;

    if (value == NULL) {
        return 0;
    }

    len = strlen(value);
    if (len != 16) {
        return 0;
    }

    for (i = 0; i < len; ++i) {
        if (!isxdigit((unsigned char)value[i])) {
            return 0;
        }
    }

    return 1;
}

/**
 * Parses trace_flags as hex string (1-2 hex characters, 0-255)
 * Returns parsed value or 0 on error
 */
static uint8_t parse_trace_flags(const char *value) {
    char *end = NULL;
    unsigned long parsed;

    if (value == NULL || value[0] == '\0') {
        return 0;
    }

    errno = 0;
    parsed = strtoul(value, &end, 16);
    if (errno != 0 || end == value || *end != '\0') {
        return 0;
    }

    if (parsed > 255) {
        return 0;
    }

    return (uint8_t)parsed;
}

/**
 * @brief Extract property value as string from message JSON variables
 *
 * Extracts a property value from the message's local JSON variables ($!).
 * The property name is looked up in the message's JSON structure. If the
 * property is a string, it is returned directly. If it's a non-string JSON
 * object, it is converted to its JSON string representation.
 *
 * @param[in] msg Rsyslog message structure
 * @param[in] prop_name Property name to extract (e.g., "trace_id")
 * @return Allocated string containing the property value (caller must free),
 *         or NULL if the property is not found or extraction fails
 */
static char *extract_property_string(smsg_t *msg, const char *prop_name) {
    struct json_object *json = NULL;
    msgPropDescr_t prop_descr;
    uchar *cstr = NULL;
    char *value = NULL;
    char *var_name = NULL;
    size_t name_len;

    if (msg == NULL || prop_name == NULL || prop_name[0] == '\0') {
        return NULL;
    }

    /* Prepend $! if not already present for local variable access */
    name_len = strlen(prop_name);
    if (name_len >= 2 && prop_name[0] == '$' && prop_name[1] == '!') {
        var_name = (char *)prop_name;
    } else {
        /* Allocate space for $! prefix + property name + null terminator */
        var_name = (char *)malloc(name_len + 3);
        if (var_name == NULL) {
            return NULL;
        }
        snprintf(var_name, name_len + 3, "$!%s", prop_name);
    }

    /* Construct property descriptor for local variable access */
    if (msgPropDescrFill(&prop_descr, (uchar *)var_name, (int)strlen(var_name)) != RS_RET_OK) {
        if (var_name != prop_name) {
            free(var_name);
        }
        return NULL;
    }

    /* Access the JSON variable from localvars */
    if (msgGetJSONPropJSONorString(msg, &prop_descr, &json, &cstr) != RS_RET_OK) {
        msgPropDescrDestruct(&prop_descr);
        if (var_name != prop_name) {
            free(var_name);
        }
        return NULL;
    }

    if (cstr != NULL && cstr[0] != '\0') {
        /* String value extracted (most common case for trace properties) */
        value = strdup((const char *)cstr);
    } else if (json != NULL) {
        /* Non-string JSON object - convert to string representation */
        const char *json_str = fjson_object_to_json_string(json);
        if (json_str != NULL && json_str[0] != '\0') {
            value = strdup(json_str);
        }
        fjson_object_put(json); /* Free the deep copy */
    }

    msgPropDescrDestruct(&prop_descr);
    if (cstr != NULL) {
        free(cstr);
    }
    if (var_name != prop_name) {
        free(var_name);
    }

    return value;
}

/**
 * @brief Populate OTLP log record from rsyslog message
 *
 * Extracts all relevant fields from the rsyslog message and populates the
 * OTLP log record structure. This includes timestamps, severity mapping,
 * hostname, application name, process ID, message ID, and trace correlation
 * data (trace_id, span_id, trace_flags).
 *
 * The function allocates memory for trace correlation strings (trace_id,
 * span_id) which must be freed by the caller.
 *
 * @param[in] msg Rsyslog message structure (may be NULL)
 * @param[in] body Log message body text
 * @param[in] pData Instance data containing configuration and property names
 * @param[out] record OTLP log record to populate
 * @return RS_RET_OK on success
 */
static rsRetVal populateLogRecord(smsg_t *msg, const char *body, instanceData *pData, omotlp_log_record_t *record) {
    int severity;
    char *trace_id_str = NULL;
    char *span_id_str = NULL;
    char *trace_flags_str = NULL;

    DEFiRet;

    memset(record, 0, sizeof(*record));

    record->body = body;

    if (msg != NULL) {
        CHKiRet(MsgGetSeverity(msg, &severity));
        mapSeverity(severity, pData, record);

        record->hostname = (msg->pszHOSTNAME != NULL) ? (const char *)msg->pszHOSTNAME : NULL;
        record->app_name = extractAppName(msg);
        record->proc_id = extractProcId(msg);
        record->msg_id = extractMsgId(msg);
        record->facility = (uint16_t)msg->iFacility;
        record->time_unix_nano = syslogTimeToUnixNanos(&msg->tTIMESTAMP);
        record->observed_time_unix_nano = syslogTimeToUnixNanos(&msg->tRcvdAt);

        /* Extract trace correlation data */
        if (pData != NULL) {
            trace_id_str = extract_property_string(msg, (const char *)pData->traceIdPropertyName);
            if (trace_id_str != NULL) {
                if (is_valid_trace_id(trace_id_str)) {
                    record->trace_id = trace_id_str;
                    trace_id_str = NULL; /* Ownership transferred */
                } else {
                    LogError(0, RS_RET_OK, "omotlp: invalid trace_id format (expected 32 hex chars): %s", trace_id_str);
                    free(trace_id_str);
                    trace_id_str = NULL;
                }
            }

            span_id_str = extract_property_string(msg, (const char *)pData->spanIdPropertyName);
            if (span_id_str != NULL) {
                if (is_valid_span_id(span_id_str)) {
                    record->span_id = span_id_str;
                    span_id_str = NULL; /* Ownership transferred */
                } else {
                    LogError(0, RS_RET_OK, "omotlp: invalid span_id format (expected 16 hex chars): %s", span_id_str);
                    free(span_id_str);
                    span_id_str = NULL;
                }
            }

            trace_flags_str = extract_property_string(msg, (const char *)pData->traceFlagsPropertyName);
            if (trace_flags_str != NULL) {
                record->trace_flags = parse_trace_flags(trace_flags_str);
                free(trace_flags_str);
                trace_flags_str = NULL;
            }
        }
    } else {
        record->severity_number = 0u;
        record->severity_text = NULL;
        record->hostname = NULL;
        record->app_name = NULL;
        record->proc_id = NULL;
        record->msg_id = NULL;
        record->facility = 0u;
        record->time_unix_nano = 0u;
        record->observed_time_unix_nano = 0u;
    }

    if (record->trace_id == NULL) {
        record->trace_id = NULL;
    }
    if (record->span_id == NULL) {
        record->span_id = NULL;
    }
    if (record->trace_flags == 0u && trace_flags_str == NULL) {
        record->trace_flags = 0u;
    }

finalize_it:
    if (trace_id_str != NULL) {
        free(trace_id_str);
    }
    if (span_id_str != NULL) {
        free(span_id_str);
    }
    if (trace_flags_str != NULL) {
        free(trace_flags_str);
    }
    RETiRet;
}

static void omotlp_batch_entry_clear(omotlp_batch_entry_t *entry) {
    if (entry == NULL) {
        return;
    }

    free(entry->body);
    free(entry->hostname);
    free(entry->app_name);
    free(entry->proc_id);
    free(entry->msg_id);
    free(entry->trace_id);
    free(entry->span_id);

    memset(entry, 0, sizeof(*entry));
}

static void omotlp_batch_clear(omotlp_batch_state_t *batch) {
    size_t i;

    if (batch == NULL) {
        return;
    }

    for (i = 0; i < batch->count; ++i) {
        omotlp_batch_entry_clear(&batch->entries[i]);
    }

    batch->count = 0u;
    batch->estimated_bytes = 0u;
    batch->first_enqueue_ms = 0;
}

static void omotlp_batch_destroy(omotlp_batch_state_t *batch) {
    if (batch == NULL) {
        return;
    }

    omotlp_batch_clear(batch);
    free(batch->entries);
    batch->entries = NULL;
    batch->capacity = 0u;
}

static rsRetVal omotlp_batch_ensure_capacity(omotlp_batch_state_t *batch, size_t needed) {
    omotlp_batch_entry_t *tmp;
    size_t new_capacity;

    DEFiRet;

    if (batch == NULL) {
        goto finalize_it;
    }

    if (needed <= batch->capacity) {
        goto finalize_it;
    }

    new_capacity = batch->capacity == 0u ? 8u : batch->capacity;
    while (new_capacity < needed) {
        new_capacity *= 2u;
    }

    tmp = (omotlp_batch_entry_t *)realloc(batch->entries, new_capacity * sizeof(omotlp_batch_entry_t));
    if (tmp == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    memset(tmp + batch->capacity, 0, (new_capacity - batch->capacity) * sizeof(omotlp_batch_entry_t));
    batch->entries = tmp;
    batch->capacity = new_capacity;

finalize_it:
    RETiRet;
}

static rsRetVal gzip_compress_buffer(const uint8_t *input, size_t input_len, uint8_t **out, size_t *out_len) {
    z_stream stream;
    uint8_t *buffer = NULL;
    size_t bound;
    int rc;

    DEFiRet;

    if (out == NULL || out_len == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    *out = NULL;
    *out_len = 0u;

    bound = (size_t)compressBound((uLong)input_len);
    buffer = (uint8_t *)malloc(bound);
    if (buffer == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    memset(&stream, 0, sizeof(stream));
    rc = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
    if (rc != Z_OK) {
        LogError(0, RS_RET_INTERNAL_ERROR, "omotlp: deflateInit2 failed: %d", rc);
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    stream.next_in = (Bytef *)input;
    stream.avail_in = (uInt)input_len;
    stream.next_out = buffer;
    stream.avail_out = (uInt)bound;

    rc = deflate(&stream, Z_FINISH);
    if (rc != Z_STREAM_END) {
        LogError(0, RS_RET_INTERNAL_ERROR, "omotlp: gzip compression failed: %d", rc);
        deflateEnd(&stream);
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    *out = buffer;
    *out_len = stream.total_out;
    buffer = NULL;

    deflateEnd(&stream);

finalize_it:
    if (buffer != NULL) {
        free(buffer);
    }
    RETiRet;
}

/**
 * @brief Flush a batch of log records to the OTLP collector
 *
 * This function must be called with the batch mutex already held. It converts
 * the batch entries to OTLP JSON format, optionally compresses the payload,
 * sends it via HTTP POST, and updates statistics based on the response.
 *
 * HTTP status code handling:
 * - 2xx: Success - batch is cleared, records.sent incremented
 * - 4xx: Client error - batch is dropped (already delivered but rejected)
 * - 5xx/408/429: Server error - batch is retained for retry
 *
 * @param[in] pWrkrData Worker instance data containing HTTP client and stats
 * @param[in,out] batch Batch state to flush (cleared on success or 4xx)
 * @return RS_RET_OK on success, RS_RET_SUSPENDED for retryable errors,
 *         RS_RET_PARAM_ERROR for invalid parameters
 */
static rsRetVal omotlp_flush_batch_locked(wrkrInstanceData_t *pWrkrData, omotlp_batch_state_t *batch) {
    omotlp_log_record_t *records = NULL;
    char *payload = NULL;
    uint8_t *compressed = NULL;
    const uint8_t *to_send;
    size_t send_len = 0u;
    size_t payload_len = 0u;
    size_t i;
    long status_code = 0;
    long latency_ms = 0;
    size_t record_count = 0u;
    omotlp_resource_attrs_t resource_attrs;

    DEFiRet;

    DBGPRINTF("omotlp: omotlp_flush_batch called, batch->count=%zu", batch ? batch->count : 0u);

    if (pWrkrData == NULL || batch == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    if (batch->count == 0u) {
        DBGPRINTF("omotlp: omotlp_flush_batch: batch is empty, skipping");
        goto finalize_it;
    }

    record_count = batch->count;
    DBGPRINTF("omotlp: omotlp_flush_batch: flushing %zu records", record_count);

    STATSCOUNTER_INC(pWrkrData->ctrBatchesSubmitted, pWrkrData->mutCtrBatchesSubmitted);

    records = (omotlp_log_record_t *)malloc(batch->count * sizeof(*records));
    if (records == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    for (i = 0; i < batch->count; ++i) {
        records[i] = batch->entries[i].record;
    }

    DBGPRINTF("omotlp: omotlp_flush_batch: building JSON export for %zu records", batch->count);
    resource_attrs = (omotlp_resource_attrs_t){
        .service_instance_id = pWrkrData->pData->resourceServiceInstanceId
                                   ? (const char *)pWrkrData->pData->resourceServiceInstanceId
                                   : NULL,
        .deployment_environment = pWrkrData->pData->resourceDeploymentEnvironment
                                      ? (const char *)pWrkrData->pData->resourceDeploymentEnvironment
                                      : NULL,
        .custom_attributes = pWrkrData->pData->resourceJsonParsed,
    };

    CHKiRet(
        omotlp_json_build_export(records, batch->count, &resource_attrs, &pWrkrData->pData->attributeMap, &payload));

    if (payload != NULL) {
        payload_len = strlen(payload);
        DBGPRINTF("omotlp: omotlp_flush_batch: JSON payload length=%zu", payload_len);
    }

    to_send = (const uint8_t *)payload;
    send_len = payload_len;

    if (pWrkrData->pData->compression_mode == OMOTLP_COMPRESSION_GZIP) {
        DBGPRINTF("omotlp: omotlp_flush_batch: compressing payload");
        CHKiRet(gzip_compress_buffer((const uint8_t *)payload, payload_len, &compressed, &send_len));
        to_send = compressed;
        DBGPRINTF("omotlp: omotlp_flush_batch: compressed size=%zu", send_len);
    }

    DBGPRINTF("omotlp: omotlp_flush_batch: calling omotlp_http_client_post, send_len=%zu", send_len);
    iRet = omotlp_http_client_post(pWrkrData->http_client, to_send, send_len, &status_code, &latency_ms);

    /* Update statistics based on HTTP response (status_code is set even on error) */
    if (status_code > 0) {
        if (status_code >= 200 && status_code < 300) {
            STATSCOUNTER_INC(pWrkrData->ctrBatchesSuccess, pWrkrData->mutCtrBatchesSuccess);
            STATSCOUNTER_ADD(pWrkrData->ctrRecordsSent, pWrkrData->mutCtrRecordsSent, record_count);
            if (latency_ms > 0) {
                STATSCOUNTER_ADD(pWrkrData->httpRequestLatencyMs, pWrkrData->mutHttpRequestLatencyMs, latency_ms);
            }
            DBGPRINTF("omotlp: omotlp_flush_batch: HTTP POST successful, clearing batch");
            omotlp_batch_clear(batch);
        } else if (status_code >= 400 && status_code < 500) {
            STATSCOUNTER_INC(pWrkrData->ctrHttpStatus4xx, pWrkrData->mutCtrHttpStatus4xx);
            if (status_code == 408 || status_code == 429) {
                /* 408 (Request Timeout) and 429 (Too Many Requests) are retryable.
                 * If the HTTP client exhausted retries and returned RS_RET_SUSPENDED,
                 * we must retain the batch so rsyslog can retry it. Otherwise, the
                 * batch data would be lost. */
                if (iRet == RS_RET_SUSPENDED) {
                    STATSCOUNTER_INC(pWrkrData->ctrBatchesRetried, pWrkrData->mutCtrBatchesRetried);
                    if (latency_ms > 0) {
                        STATSCOUNTER_ADD(pWrkrData->httpRequestLatencyMs, pWrkrData->mutHttpRequestLatencyMs,
                                         latency_ms);
                    }
                    DBGPRINTF("omotlp: omotlp_flush_batch: HTTP %ld error, retaining batch for retry", status_code);
                    /* Don't clear batch - it will be retried by rsyslog */
                } else {
                    STATSCOUNTER_INC(pWrkrData->ctrBatchesDropped, pWrkrData->mutCtrBatchesDropped);
                    if (latency_ms > 0) {
                        STATSCOUNTER_ADD(pWrkrData->httpRequestLatencyMs, pWrkrData->mutHttpRequestLatencyMs,
                                         latency_ms);
                    }
                    DBGPRINTF("omotlp: omotlp_flush_batch: HTTP %ld error, clearing batch", status_code);
                    omotlp_batch_clear(batch);
                }
            } else {
                /* Non-retryable 4xx (400, 401, 403, 404, etc.) - clear batch */
                STATSCOUNTER_INC(pWrkrData->ctrBatchesDropped, pWrkrData->mutCtrBatchesDropped);
                if (latency_ms > 0) {
                    STATSCOUNTER_ADD(pWrkrData->httpRequestLatencyMs, pWrkrData->mutHttpRequestLatencyMs, latency_ms);
                }
                DBGPRINTF("omotlp: omotlp_flush_batch: HTTP 4xx error, clearing batch");
                omotlp_batch_clear(batch);
                /* For non-retryable 4xx errors, clear the error so the caller can continue.
                 * The batch has been dropped, but new records should still be accepted. */
                if (iRet == RS_RET_DISCARDMSG) {
                    iRet = RS_RET_OK;
                }
            }
        } else if (status_code >= 500) {
            STATSCOUNTER_INC(pWrkrData->ctrHttpStatus5xx, pWrkrData->mutCtrHttpStatus5xx);
            STATSCOUNTER_INC(pWrkrData->ctrBatchesRetried, pWrkrData->mutCtrBatchesRetried);
            if (latency_ms > 0) {
                STATSCOUNTER_ADD(pWrkrData->httpRequestLatencyMs, pWrkrData->mutHttpRequestLatencyMs, latency_ms);
            }
            /* Don't clear batch on 5xx - it will be retried */
        } else {
            /* Other status codes (1xx, 3xx) - track latency if available */
            if (latency_ms > 0) {
                STATSCOUNTER_ADD(pWrkrData->httpRequestLatencyMs, pWrkrData->mutHttpRequestLatencyMs, latency_ms);
            }
        }
    } else if (latency_ms > 0) {
        /* Network error or other failure - still track latency if available */
        STATSCOUNTER_ADD(pWrkrData->httpRequestLatencyMs, pWrkrData->mutHttpRequestLatencyMs, latency_ms);
    }

    if (iRet != RS_RET_OK) {
        CHKiRet(iRet);
    }

finalize_it:
    free(records);
    free(payload);
    free(compressed);
    RETiRet;
}

static rsRetVal omotlp_flush_batch(wrkrInstanceData_t *pWrkrData) {
    rsRetVal iRet;

    if (pWrkrData == NULL) {
        return RS_RET_PARAM_ERROR;
    }

    pthread_mutex_lock(&pWrkrData->batch_mutex);
    iRet = omotlp_flush_batch_locked(pWrkrData, &pWrkrData->batch);
    pthread_mutex_unlock(&pWrkrData->batch_mutex);

    return iRet;
}

/**
 * @brief Background thread for timeout-based batch flushing
 *
 * This thread runs in the background and periodically checks if any batches
 * have exceeded their timeout threshold. It wakes every 100ms to check for
 * batches that need to be flushed due to timeout. The thread performs a final
 * flush of any remaining data before exiting when the stop flag is set.
 *
 * @param[in] arg Worker instance data pointer (cast from void*)
 * @return NULL (pthread requirement)
 */
static void *omotlp_batch_flush_thread(void *arg) {
    wrkrInstanceData_t *pWrkrData = (wrkrInstanceData_t *)arg;
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = 100 * 1000 * 1000; /* 100 ms */

    for (;;) {
        nanosleep(&req, NULL);

        pthread_mutex_lock(&pWrkrData->batch_mutex);
        if (pWrkrData->flush_thread_stop) {
            pthread_mutex_unlock(&pWrkrData->batch_mutex);
            break;
        }

        if (pWrkrData->batch.count > 0u) {
            long long timeout_ms =
                pWrkrData->pData->batchTimeoutMs > 0 ? pWrkrData->pData->batchTimeoutMs : OMOTLP_IDLE_FLUSH_INTERVAL_MS;
            if (timeout_ms > 0) {
                long long now = currentTimeMills();
                if (pWrkrData->batch.first_enqueue_ms != 0 && now - pWrkrData->batch.first_enqueue_ms >= timeout_ms) {
                    /* Check stop flag before starting potentially long-running flush operation */
                    if (!pWrkrData->flush_thread_stop) {
                        (void)omotlp_flush_batch_locked(pWrkrData, &pWrkrData->batch);
                    }
                }
            }
        }

        pthread_mutex_unlock(&pWrkrData->batch_mutex);
    }

    pthread_mutex_lock(&pWrkrData->batch_mutex);
    /* Final flush before thread exit - flush any remaining data */
    if (pWrkrData->batch.count > 0u) {
        (void)omotlp_flush_batch_locked(pWrkrData, &pWrkrData->batch);
    }
    pthread_mutex_unlock(&pWrkrData->batch_mutex);

    return NULL;
}

/**
 * @brief Add a log record to the batch buffer
 *
 * Adds a log record to the worker's batch buffer. The function automatically
 * flushes the batch if thresholds are reached (max_items, max_bytes) before
 * adding the new record. The batch mutex is acquired internally and released
 * on return.
 *
 * The function performs rollback of batch state if allocation fails after
 * incrementing the count, ensuring consistency.
 *
 * @param[in] pWrkrData Worker instance data
 * @param[in] record Log record to add to the batch
 * @param[in] body Log message body (duplicated for batch entry)
 * @return RS_RET_OK on success, RS_RET_PARAM_ERROR for invalid parameters,
 *         RS_RET_OUT_OF_MEMORY on allocation failure
 */
static rsRetVal omotlp_batch_add_record(wrkrInstanceData_t *pWrkrData,
                                        const omotlp_log_record_t *record,
                                        const char *body) {
    omotlp_batch_state_t *batch;
    instanceData *cfg;
    omotlp_batch_entry_t *entry = NULL;
    const char *body_text;
    size_t body_len;
    size_t estimated_bytes;
    int mutex_locked = 0;
    int count_incremented = 0;
    int estimated_bytes_updated = 0;

    DEFiRet;

    if (pWrkrData == NULL || record == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    batch = &pWrkrData->batch;
    cfg = pWrkrData->pData;

    pthread_mutex_lock(&pWrkrData->batch_mutex);
    mutex_locked = 1;

    if (cfg->batchMaxItems > 0u && batch->count >= cfg->batchMaxItems) {
        CHKiRet(omotlp_flush_batch_locked(pWrkrData, batch));
    }

    body_text = (body != NULL) ? body : "";
    body_len = strlen(body_text);
    estimated_bytes = OMOTLP_BATCH_RECORD_OVERHEAD + body_len;

    if (cfg->batchMaxBytes > 0u && batch->count > 0u && batch->estimated_bytes + estimated_bytes > cfg->batchMaxBytes) {
        CHKiRet(omotlp_flush_batch_locked(pWrkrData, batch));
    }

    if (cfg->batchMaxBytes > 0u && estimated_bytes > cfg->batchMaxBytes) {
        LogError(0, RS_RET_OK,
                 "omotlp: single record estimated size %zu exceeds batch.max_bytes %zu; sending individually",
                 estimated_bytes, cfg->batchMaxBytes);
    }

    CHKiRet(omotlp_batch_ensure_capacity(batch, batch->count + 1u));
    entry = &batch->entries[batch->count];
    memset(entry, 0, sizeof(*entry));

    entry->body = strdup(body_text);
    if (entry->body == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    CHKiRet(duplicate_optional_string(record->hostname, &entry->hostname));
    CHKiRet(duplicate_optional_string(record->app_name, &entry->app_name));
    CHKiRet(duplicate_optional_string(record->proc_id, &entry->proc_id));
    CHKiRet(duplicate_optional_string(record->msg_id, &entry->msg_id));

    entry->record = *record;
    entry->record.body = entry->body;
    entry->record.hostname = entry->hostname;
    entry->record.app_name = entry->app_name;
    entry->record.proc_id = entry->proc_id;
    entry->record.msg_id = entry->msg_id;

    /* Store trace correlation strings */
    if (record->trace_id != NULL) {
        CHKiRet(duplicate_optional_string(record->trace_id, &entry->trace_id));
        entry->record.trace_id = entry->trace_id;
    }
    if (record->span_id != NULL) {
        CHKiRet(duplicate_optional_string(record->span_id, &entry->span_id));
        entry->record.span_id = entry->span_id;
    }
    entry->record.trace_flags = record->trace_flags;

    ++batch->count;
    count_incremented = 1;
    if (batch->count == 1u) {
        batch->estimated_bytes = OMOTLP_BATCH_BASE_OVERHEAD + estimated_bytes;
        batch->first_enqueue_ms = currentTimeMills();
        estimated_bytes_updated = 1;
    } else {
        batch->estimated_bytes += estimated_bytes;
        estimated_bytes_updated = 1;
    }

    if (cfg->batchMaxItems > 0u && batch->count >= cfg->batchMaxItems) {
        CHKiRet(omotlp_flush_batch_locked(pWrkrData, batch));
    } else if (cfg->batchMaxBytes > 0u && batch->estimated_bytes >= cfg->batchMaxBytes) {
        CHKiRet(omotlp_flush_batch_locked(pWrkrData, batch));
    }

finalize_it:
    if (iRet != RS_RET_OK) {
        if (mutex_locked) {
            /* Rollback batch count increment if we incremented it */
            if (count_incremented && batch != NULL && batch->count > 0u) {
                --batch->count;
                /* Rollback estimated_bytes increment if we updated it */
                if (estimated_bytes_updated) {
                    if (batch->count == 0u) {
                        /* Rolling back the first record: reset estimated_bytes */
                        batch->estimated_bytes = 0u;
                    } else {
                        /* Rolling back a subsequent record: subtract the added bytes */
                        batch->estimated_bytes -= estimated_bytes;
                    }
                }
            }
            /* Cleanup entry if it was allocated */
            if (entry != NULL) {
                omotlp_batch_entry_clear(entry);
            }
        } else {
            /* Cleanup entry without mutex if mutex was never acquired */
            if (entry != NULL) {
                omotlp_batch_entry_clear(entry);
            }
        }
    }
    if (mutex_locked) {
        pthread_mutex_unlock(&pWrkrData->batch_mutex);
    }
    RETiRet;
}

static rsRetVal omotlp_batch_flush_if_due(wrkrInstanceData_t *pWrkrData, long long now_ms) {
    omotlp_batch_state_t *batch;
    long long age;
    int mutex_locked = 0;

    DEFiRet;

    if (pWrkrData == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    pthread_mutex_lock(&pWrkrData->batch_mutex);
    mutex_locked = 1;
    batch = &pWrkrData->batch;
    if (pWrkrData->pData->batchTimeoutMs <= 0 || batch->count == 0u) {
        goto finalize_it;
    }

    if (now_ms <= batch->first_enqueue_ms) {
        goto finalize_it;
    }

    age = now_ms - batch->first_enqueue_ms;
    if (age >= pWrkrData->pData->batchTimeoutMs) {
        CHKiRet(omotlp_flush_batch_locked(pWrkrData, batch));
    }

finalize_it:
    if (mutex_locked) {
        pthread_mutex_unlock(&pWrkrData->batch_mutex);
    }
    RETiRet;
}

static inline void setInstParamDefaults(instanceData *pData) {
    pData->endpoint = NULL;
    pData->path = NULL;
    pData->protocol = NULL;
    pData->bodyTemplateName = NULL;
    pData->url = NULL;
    pData->requestTimeoutMs = 10000;
    pData->batchMaxItems = OMOTLP_DEFAULT_BATCH_MAX_ITEMS;
    pData->batchMaxBytes = OMOTLP_DEFAULT_BATCH_MAX_BYTES;
    pData->batchTimeoutMs = OMOTLP_DEFAULT_BATCH_TIMEOUT_MS;
    pData->retryInitialMs = OMOTLP_DEFAULT_RETRY_INITIAL_MS;
    pData->retryMaxMs = OMOTLP_DEFAULT_RETRY_MAX_MS;
    pData->retryMaxRetries = OMOTLP_DEFAULT_RETRY_MAX_RETRIES;
    pData->retryJitterPercent = OMOTLP_DEFAULT_RETRY_JITTER_PERCENT;
    pData->compression_mode = OMOTLP_COMPRESSION_UNSET;
    pData->compressionConfigured = 0;
    pData->headersConfigured = 0;
    pData->bearerConfigured = 0;
    pData->timeoutConfigured = 0;
    header_list_init(&pData->headers);
    pData->resourceServiceInstanceId = NULL;
    pData->resourceDeploymentEnvironment = NULL;
    pData->resourceJson = NULL;
    pData->resourceJsonParsed = NULL;
    /* TLS defaults */
    pData->tlsCaCertFile = NULL;
    pData->tlsCaCertDir = NULL;
    pData->tlsClientCertFile = NULL;
    pData->tlsClientKeyFile = NULL;
    pData->tlsVerifyHostname = 1; /* Default: verify hostname */
    pData->tlsVerifyPeer = 1; /* Default: verify peer certificate */
    /* Proxy defaults */
    pData->proxyUrl = NULL;
    pData->proxyUser = NULL;
    pData->proxyPassword = NULL;
    /* Trace correlation defaults */
    pData->traceIdPropertyName = NULL; /* Will default to "trace_id" */
    pData->spanIdPropertyName = NULL; /* Will default to "span_id" */
    pData->traceFlagsPropertyName = NULL; /* Will default to "trace_flags" */
    /* Custom mappings */
    attribute_map_init(&pData->attributeMap);
    pData->severityMap.configured = 0;
    for (int i = 0; i < 8; ++i) {
        pData->severityMap.entries[i].severity_text = NULL;
    }
}

BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
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
    setInstParamDefaults(pData);
ENDcreateInstance

BEGINcreateWrkrInstance
    omotlp_http_client_config_t http_cfg;
    char stats_name[256];
    CODESTARTcreateWrkrInstance;
    pWrkrData->pData = pData;
    pWrkrData->http_client = NULL;
    pWrkrData->batch.entries = NULL;
    pWrkrData->batch.count = 0u;
    pWrkrData->batch.capacity = 0u;
    pWrkrData->batch.estimated_bytes = 0u;
    pWrkrData->batch.first_enqueue_ms = 0;
    pWrkrData->flush_thread_running = 0;
    pWrkrData->flush_thread_stop = 0;
    pWrkrData->stats = NULL;
    pthread_mutex_init(&pWrkrData->batch_mutex, NULL);

    if (pData == NULL || pData->url == NULL) {
        iRet = RS_RET_INTERNAL_ERROR;
        goto finalize_it;
    }

    http_cfg.url = (const char *)pData->url;
    http_cfg.timeout_ms = pData->requestTimeoutMs;
    http_cfg.user_agent = "rsyslog-omotlp/" VERSION;
    http_cfg.headers = (const char *const *)pData->headers.values;
    http_cfg.header_count = pData->headers.count;
    http_cfg.retry_initial_ms = pData->retryInitialMs;
    http_cfg.retry_max_ms = pData->retryMaxMs;
    http_cfg.retry_max_retries = pData->retryMaxRetries;
    http_cfg.retry_jitter_percent = pData->retryJitterPercent;

    /* TLS configuration */
    http_cfg.tls_ca_cert_file = (const char *)pData->tlsCaCertFile;
    http_cfg.tls_ca_cert_dir = (const char *)pData->tlsCaCertDir;
    http_cfg.tls_client_cert_file = (const char *)pData->tlsClientCertFile;
    http_cfg.tls_client_key_file = (const char *)pData->tlsClientKeyFile;
    http_cfg.tls_verify_hostname = pData->tlsVerifyHostname;
    http_cfg.tls_verify_peer = pData->tlsVerifyPeer;

    /* Proxy configuration */
    http_cfg.proxy_url = (const char *)pData->proxyUrl;
    http_cfg.proxy_user = (const char *)pData->proxyUser;
    http_cfg.proxy_password = (const char *)pData->proxyPassword;

    iRet = omotlp_http_client_create(&http_cfg, &pWrkrData->http_client);
    if (iRet != RS_RET_OK) {
        goto finalize_it;
    }

    /* Initialize statistics */
    snprintf(stats_name, sizeof(stats_name), "omotlp-%s", pData->url ? (char *)pData->url : "default");
    stats_name[sizeof(stats_name) - 1] = '\0';
    CHKiRet(statsobj.Construct(&pWrkrData->stats));
    CHKiRet(statsobj.SetName(pWrkrData->stats, (uchar *)stats_name));
    CHKiRet(statsobj.SetOrigin(pWrkrData->stats, (uchar *)"omotlp"));

    STATSCOUNTER_INIT(pWrkrData->ctrBatchesSubmitted, pWrkrData->mutCtrBatchesSubmitted);
    CHKiRet(statsobj.AddCounter(pWrkrData->stats, (uchar *)"batches.submitted", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pWrkrData->ctrBatchesSubmitted)));

    STATSCOUNTER_INIT(pWrkrData->ctrBatchesSuccess, pWrkrData->mutCtrBatchesSuccess);
    CHKiRet(statsobj.AddCounter(pWrkrData->stats, (uchar *)"batches.success", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pWrkrData->ctrBatchesSuccess)));

    STATSCOUNTER_INIT(pWrkrData->ctrBatchesRetried, pWrkrData->mutCtrBatchesRetried);
    CHKiRet(statsobj.AddCounter(pWrkrData->stats, (uchar *)"batches.retried", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pWrkrData->ctrBatchesRetried)));

    STATSCOUNTER_INIT(pWrkrData->ctrBatchesDropped, pWrkrData->mutCtrBatchesDropped);
    CHKiRet(statsobj.AddCounter(pWrkrData->stats, (uchar *)"batches.dropped", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pWrkrData->ctrBatchesDropped)));

    STATSCOUNTER_INIT(pWrkrData->ctrHttpStatus4xx, pWrkrData->mutCtrHttpStatus4xx);
    CHKiRet(statsobj.AddCounter(pWrkrData->stats, (uchar *)"http.status.4xx", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pWrkrData->ctrHttpStatus4xx)));

    STATSCOUNTER_INIT(pWrkrData->ctrHttpStatus5xx, pWrkrData->mutCtrHttpStatus5xx);
    CHKiRet(statsobj.AddCounter(pWrkrData->stats, (uchar *)"http.status.5xx", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pWrkrData->ctrHttpStatus5xx)));

    STATSCOUNTER_INIT(pWrkrData->ctrRecordsSent, pWrkrData->mutCtrRecordsSent);
    CHKiRet(statsobj.AddCounter(pWrkrData->stats, (uchar *)"records.sent", ctrType_IntCtr, CTR_FLAG_RESETTABLE,
                                &(pWrkrData->ctrRecordsSent)));

    STATSCOUNTER_INIT(pWrkrData->httpRequestLatencyMs, pWrkrData->mutHttpRequestLatencyMs);
    CHKiRet(statsobj.AddCounter(pWrkrData->stats, (uchar *)"http.request.latency.ms", ctrType_IntCtr,
                                CTR_FLAG_RESETTABLE, &(pWrkrData->httpRequestLatencyMs)));

    CHKiRet(statsobj.ConstructFinalize(pWrkrData->stats));

    if (pthread_create(&pWrkrData->flush_thread, NULL, omotlp_batch_flush_thread, pWrkrData) != 0) {
        LogError(errno, RS_RET_SYS_ERR, "omotlp: failed to create flush thread");
        iRet = RS_RET_SYS_ERR;
        goto finalize_it;
    }
    pWrkrData->flush_thread_running = 1;

finalize_it:
    if (iRet != RS_RET_OK) {
        if (pWrkrData != NULL) {
            if (pWrkrData->flush_thread_running) {
                /* Acquire mutex before setting stop flag to synchronize with flush thread */
                pthread_mutex_lock(&pWrkrData->batch_mutex);
                pWrkrData->flush_thread_stop = 1;
                pthread_mutex_unlock(&pWrkrData->batch_mutex);
                pthread_join(pWrkrData->flush_thread, NULL);
                pWrkrData->flush_thread_running = 0;
            }
            if (pWrkrData->stats != NULL) {
                statsobj.Destruct(&pWrkrData->stats);
            }
            pthread_mutex_destroy(&pWrkrData->batch_mutex);
        }
        omotlp_http_client_destroy(&pWrkrData->http_client);
        free(pWrkrData);
        pWrkrData = NULL;
    }
ENDcreateWrkrInstance

BEGINfreeInstance
    CODESTARTfreeInstance;
    if (pData != NULL) {
        free(pData->endpoint);
        free(pData->path);
        free(pData->protocol);
        free(pData->bodyTemplateName);
        free(pData->url);
        header_list_destroy(&pData->headers);
        free(pData->resourceServiceInstanceId);
        free(pData->resourceDeploymentEnvironment);
        free(pData->resourceJson);
        if (pData->resourceJsonParsed != NULL) {
            fjson_object_put(pData->resourceJsonParsed);
            pData->resourceJsonParsed = NULL;
        }
        free(pData->traceIdPropertyName);
        free(pData->spanIdPropertyName);
        free(pData->traceFlagsPropertyName);
        attribute_map_destroy(&pData->attributeMap);
        free(pData->tlsCaCertFile);
        free(pData->tlsCaCertDir);
        free(pData->tlsClientCertFile);
        free(pData->tlsClientKeyFile);
        free(pData->proxyUrl);
        free(pData->proxyUser);
        free(pData->proxyPassword);
        for (int i = 0; i < 8; ++i) {
            free(pData->severityMap.entries[i].severity_text);
        }
    }
ENDfreeInstance

BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
    if (pWrkrData != NULL) {
        if (pWrkrData->flush_thread_running) {
            /* Acquire mutex before setting stop flag to synchronize with flush thread */
            pthread_mutex_lock(&pWrkrData->batch_mutex);
            pWrkrData->flush_thread_stop = 1;
            pthread_mutex_unlock(&pWrkrData->batch_mutex);
            pthread_join(pWrkrData->flush_thread, NULL);
            pWrkrData->flush_thread_running = 0;
        }
        (void)omotlp_flush_batch(pWrkrData);
        omotlp_batch_destroy(&pWrkrData->batch);
        pthread_mutex_destroy(&pWrkrData->batch_mutex);
        omotlp_http_client_destroy(&pWrkrData->http_client);
        if (pWrkrData->stats != NULL) {
            statsobj.Destruct(&pWrkrData->stats);
        }
    }
ENDfreeWrkrInstance

BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    dbgprintf("omotlp\n");
    dbgprintf("\tendpoint='%s'\n", pData->endpoint ? (char *)pData->endpoint : "(default)");
    dbgprintf("\tpath='%s'\n", pData->path ? (char *)pData->path : "(default)");
    dbgprintf("\tprotocol='%s'\n", pData->protocol ? (char *)pData->protocol : "(default)");
    dbgprintf("\ttemplate='%s'\n", pData->bodyTemplateName ? (char *)pData->bodyTemplateName : "RSYSLOG_FileFormat");
    dbgprintf("\turl='%s'\n", pData->url ? (char *)pData->url : "(unresolved)");
    dbgprintf("\ttimeout.ms=%ld\n", pData->requestTimeoutMs);
    dbgprintf("\tbatch.max_items=%zu\n", pData->batchMaxItems);
    dbgprintf("\tbatch.max_bytes=%zu\n", pData->batchMaxBytes);
    dbgprintf("\tbatch.timeout.ms=%ld\n", pData->batchTimeoutMs);
    dbgprintf("\tretry.initial.ms=%ld\n", pData->retryInitialMs);
    dbgprintf("\tretry.max.ms=%ld\n", pData->retryMaxMs);
    dbgprintf("\tretry.max_retries=%u\n", pData->retryMaxRetries);
    dbgprintf("\tretry.jitter.percent=%u\n", pData->retryJitterPercent);
    dbgprintf("\tcompression=%s\n", pData->compression_mode == OMOTLP_COMPRESSION_GZIP ? "gzip" : "none");
ENDdbgPrintInstInfo

BEGINtryResume
    CODESTARTtryResume;
ENDtryResume

static rsRetVal assignParamFromEStr(uchar **target, es_str_t *value) {
    uchar *tmp;
    DEFiRet;

    free(*target);
    *target = NULL;
    tmp = (uchar *)es_str2cstr(value, NULL);
    if (tmp == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    *target = tmp;

finalize_it:
    RETiRet;
}

BEGINnewActInst
    struct cnfparamvals *pvals;
    int i;
    uchar *tplToUse = NULL;
    char *bearer_token = NULL;
    char *bearer_auth = NULL;
    char *resource_json_text = NULL;
    struct json_object *resource_parsed = NULL;
    char *attribute_map_json_text = NULL;
    char *severity_map_json_text = NULL;
    char *text = NULL;
    CODESTARTnewActInst;

    if ((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS, "omotlp: error reading config parameters");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    CHKiRet(createInstance(&pData));

    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;

        if (!strcmp(actpblk.descr[i].name, "endpoint")) {
            CHKiRet(assignParamFromEStr(&pData->endpoint, pvals[i].val.d.estr));
        } else if (!strcmp(actpblk.descr[i].name, "path")) {
            CHKiRet(assignParamFromEStr(&pData->path, pvals[i].val.d.estr));
        } else if (!strcmp(actpblk.descr[i].name, "protocol")) {
            CHKiRet(assignParamFromEStr(&pData->protocol, pvals[i].val.d.estr));
        } else if (!strcmp(actpblk.descr[i].name, "template")) {
            CHKiRet(assignParamFromEStr(&pData->bodyTemplateName, pvals[i].val.d.estr));
        } else if (!strcmp(actpblk.descr[i].name, "timeout.ms")) {
            long timeout_ms = 0;
            CHKiRet(parse_long_param("timeout.ms", pvals[i].val.d.estr, 0, &timeout_ms));
            pData->requestTimeoutMs = timeout_ms;
            pData->timeoutConfigured = 1;
        } else if (!strcmp(actpblk.descr[i].name, "compression")) {
            text = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
            if (text == NULL) {
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            CHKiRet(set_compression_mode(pData, text));
            pData->compressionConfigured = 1;
            free(text);
            text = NULL;
        } else if (!strcmp(actpblk.descr[i].name, "batch.max_items")) {
            size_t max_items = 0u;
            CHKiRet(parse_size_param("batch.max_items", pvals[i].val.d.estr, 0u, &max_items));
            pData->batchMaxItems = max_items;
        } else if (!strcmp(actpblk.descr[i].name, "batch.max_bytes")) {
            size_t max_bytes = 0u;
            CHKiRet(parse_size_param("batch.max_bytes", pvals[i].val.d.estr, 0u, &max_bytes));
            pData->batchMaxBytes = max_bytes;
        } else if (!strcmp(actpblk.descr[i].name, "batch.timeout.ms")) {
            long timeout_ms = 0;
            CHKiRet(parse_long_param("batch.timeout.ms", pvals[i].val.d.estr, 0, &timeout_ms));
            pData->batchTimeoutMs = timeout_ms;
        } else if (!strcmp(actpblk.descr[i].name, "retry.initial.ms")) {
            long backoff = 0;
            CHKiRet(parse_long_param("retry.initial.ms", pvals[i].val.d.estr, 0, &backoff));
            pData->retryInitialMs = backoff;
        } else if (!strcmp(actpblk.descr[i].name, "retry.max.ms")) {
            long max_backoff = 0;
            CHKiRet(parse_long_param("retry.max.ms", pvals[i].val.d.estr, 0, &max_backoff));
            pData->retryMaxMs = max_backoff;
        } else if (!strcmp(actpblk.descr[i].name, "retry.max_retries")) {
            unsigned int retries = 0u;
            CHKiRet(parse_uint_param("retry.max_retries", pvals[i].val.d.estr, 0u, &retries));
            pData->retryMaxRetries = retries;
        } else if (!strcmp(actpblk.descr[i].name, "retry.jitter.percent")) {
            unsigned int jitter = 0u;
            CHKiRet(parse_uint_param("retry.jitter.percent", pvals[i].val.d.estr, 0u, &jitter));
            if (jitter > 100u) {
                LogError(0, RS_RET_PARAM_ERROR, "omotlp: retry.jitter.percent must be between 0 and 100");
                ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            }
            pData->retryJitterPercent = jitter;
        } else if (!strcmp(actpblk.descr[i].name, "headers")) {
            text = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
            if (text == NULL) {
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            CHKiRet(parse_headers_json(pData, text));
            pData->headersConfigured = 1;
            free(text);
            text = NULL;
        } else if (!strcmp(actpblk.descr[i].name, "bearer_token")) {
            bearer_token = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
            if (bearer_token == NULL) {
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            bearer_auth = (char *)malloc(strlen(bearer_token) + strlen("Bearer ") + 1u);
            if (bearer_auth == NULL) {
                free(bearer_token);
                bearer_token = NULL;
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            snprintf(bearer_auth, strlen(bearer_token) + sizeof("Bearer "), "Bearer %s", bearer_token);
            CHKiRet(header_list_add_kv(&pData->headers, "Authorization", bearer_auth));
            pData->bearerConfigured = 1;
            /* Free local allocations after successful header addition */
            /* header_list_add_kv copies the string, so we can free immediately */
            free(bearer_auth);
            bearer_auth = NULL;
            free(bearer_token);
            bearer_token = NULL;
        } else if (!strcmp(actpblk.descr[i].name, "resource.service_instance_id")) {
            CHKiRet(assignParamFromEStr(&pData->resourceServiceInstanceId, pvals[i].val.d.estr));
        } else if (!strcmp(actpblk.descr[i].name, "resource.deployment.environment")) {
            CHKiRet(assignParamFromEStr(&pData->resourceDeploymentEnvironment, pvals[i].val.d.estr));
        } else if (!strcmp(actpblk.descr[i].name, "resource")) {
            resource_json_text = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);

            if (resource_json_text == NULL) {
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }

            /* Parse and validate JSON */
            resource_parsed = fjson_tokener_parse(resource_json_text);
            if (resource_parsed == NULL) {
                LogError(0, RS_RET_PARAM_ERROR, "omotlp: resource parameter contains invalid JSON: %s",
                         resource_json_text);
                free(resource_json_text);
                resource_json_text = NULL;
                ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            }

            if (!fjson_object_is_type(resource_parsed, fjson_type_object)) {
                LogError(0, RS_RET_PARAM_ERROR, "omotlp: resource parameter must be a JSON object");
                fjson_object_put(resource_parsed);
                resource_parsed = NULL;
                free(resource_json_text);
                resource_json_text = NULL;
                ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            }

            /* Store both string and parsed object */
            CHKiRet(assignParamFromCStr(&pData->resourceJson, resource_json_text));
            pData->resourceJsonParsed = resource_parsed;
            resource_parsed = NULL; /* Ownership transferred */
            free(resource_json_text);
            resource_json_text = NULL;
        } else if (!strcmp(actpblk.descr[i].name, "trace_id.property")) {
            CHKiRet(assignParamFromEStr(&pData->traceIdPropertyName, pvals[i].val.d.estr));
        } else if (!strcmp(actpblk.descr[i].name, "span_id.property")) {
            CHKiRet(assignParamFromEStr(&pData->spanIdPropertyName, pvals[i].val.d.estr));
        } else if (!strcmp(actpblk.descr[i].name, "trace_flags.property")) {
            CHKiRet(assignParamFromEStr(&pData->traceFlagsPropertyName, pvals[i].val.d.estr));
        } else if (!strcmp(actpblk.descr[i].name, "attributeMap")) {
            attribute_map_json_text = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
            if (attribute_map_json_text == NULL) {
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            CHKiRet(parse_attribute_map(pData, attribute_map_json_text));
            free(attribute_map_json_text);
            attribute_map_json_text = NULL;
        } else if (!strcmp(actpblk.descr[i].name, "severity.map")) {
            severity_map_json_text = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
            if (severity_map_json_text == NULL) {
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            CHKiRet(parse_severity_map(pData, severity_map_json_text));
            free(severity_map_json_text);
            severity_map_json_text = NULL;
        } else if (!strcmp(actpblk.descr[i].name, "tls.cacert")) {
            CHKiRet(assignParamFromEStr(&pData->tlsCaCertFile, pvals[i].val.d.estr));
            /* Validate file exists and is readable */
            FILE *fp = fopen((const char *)pData->tlsCaCertFile, "r");
            if (fp == NULL) {
                char errStr[1024];
                rs_strerror_r(errno, errStr, sizeof(errStr));
                LogError(0, RS_RET_NO_FILE_ACCESS, "omotlp: tls.cacert file '%s' cannot be accessed: %s",
                         pData->tlsCaCertFile, errStr);
                ABORT_FINALIZE(RS_RET_NO_FILE_ACCESS);
            }
            fclose(fp);
        } else if (!strcmp(actpblk.descr[i].name, "tls.cadir")) {
            CHKiRet(assignParamFromEStr(&pData->tlsCaCertDir, pvals[i].val.d.estr));
            /* Validate directory exists */
            DIR *dir = opendir((const char *)pData->tlsCaCertDir);
            if (dir == NULL) {
                char errStr[1024];
                rs_strerror_r(errno, errStr, sizeof(errStr));
                LogError(0, RS_RET_NO_FILE_ACCESS, "omotlp: tls.cadir directory '%s' cannot be accessed: %s",
                         pData->tlsCaCertDir, errStr);
                ABORT_FINALIZE(RS_RET_NO_FILE_ACCESS);
            }
            closedir(dir);
        } else if (!strcmp(actpblk.descr[i].name, "tls.cert")) {
            CHKiRet(assignParamFromEStr(&pData->tlsClientCertFile, pvals[i].val.d.estr));
            /* Validate file exists and is readable */
            FILE *fp = fopen((const char *)pData->tlsClientCertFile, "r");
            if (fp == NULL) {
                char errStr[1024];
                rs_strerror_r(errno, errStr, sizeof(errStr));
                LogError(0, RS_RET_NO_FILE_ACCESS, "omotlp: tls.cert file '%s' cannot be accessed: %s",
                         pData->tlsClientCertFile, errStr);
                ABORT_FINALIZE(RS_RET_NO_FILE_ACCESS);
            }
            fclose(fp);
        } else if (!strcmp(actpblk.descr[i].name, "tls.key")) {
            CHKiRet(assignParamFromEStr(&pData->tlsClientKeyFile, pvals[i].val.d.estr));
            /* Validate file exists and is readable */
            FILE *fp = fopen((const char *)pData->tlsClientKeyFile, "r");
            if (fp == NULL) {
                char errStr[1024];
                rs_strerror_r(errno, errStr, sizeof(errStr));
                LogError(0, RS_RET_NO_FILE_ACCESS, "omotlp: tls.key file '%s' cannot be accessed: %s",
                         pData->tlsClientKeyFile, errStr);
                ABORT_FINALIZE(RS_RET_NO_FILE_ACCESS);
            }
            fclose(fp);
        } else if (!strcmp(actpblk.descr[i].name, "tls.verify_hostname")) {
            text = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
            if (text == NULL) {
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            lowercaseInPlace((uchar *)text);
            if (!strcmp(text, "on") || !strcmp(text, "yes") || !strcmp(text, "1")) {
                pData->tlsVerifyHostname = 1;
            } else if (!strcmp(text, "off") || !strcmp(text, "no") || !strcmp(text, "0")) {
                pData->tlsVerifyHostname = 0;
            } else {
                LogError(0, RS_RET_PARAM_ERROR, "omotlp: tls.verify_hostname must be 'on' or 'off'");
                free(text);
                text = NULL;
                ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            }
            free(text);
            text = NULL;
        } else if (!strcmp(actpblk.descr[i].name, "tls.verify_peer")) {
            text = (char *)es_str2cstr(pvals[i].val.d.estr, NULL);
            if (text == NULL) {
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            lowercaseInPlace((uchar *)text);
            if (!strcmp(text, "on") || !strcmp(text, "yes") || !strcmp(text, "1")) {
                pData->tlsVerifyPeer = 1;
            } else if (!strcmp(text, "off") || !strcmp(text, "no") || !strcmp(text, "0")) {
                pData->tlsVerifyPeer = 0;
            } else {
                LogError(0, RS_RET_PARAM_ERROR, "omotlp: tls.verify_peer must be 'on' or 'off'");
                free(text);
                text = NULL;
                ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            }
            free(text);
            text = NULL;
        } else if (!strcmp(actpblk.descr[i].name, "proxy")) {
            CHKiRet(assignParamFromEStr(&pData->proxyUrl, pvals[i].val.d.estr));
            /* Validate URL format */
            if (pData->proxyUrl != NULL) {
                if (strncmp((char *)pData->proxyUrl, "http://", 7) != 0 &&
                    strncmp((char *)pData->proxyUrl, "https://", 8) != 0 &&
                    strncmp((char *)pData->proxyUrl, "socks4://", 9) != 0 &&
                    strncmp((char *)pData->proxyUrl, "socks5://", 9) != 0) {
                    LogError(0, RS_RET_PARAM_ERROR,
                             "omotlp: proxy URL must start with http://, https://, socks4://, or socks5://");
                    ABORT_FINALIZE(RS_RET_PARAM_ERROR);
                }
            }
        } else if (!strcmp(actpblk.descr[i].name, "proxy.user")) {
            CHKiRet(assignParamFromEStr(&pData->proxyUser, pvals[i].val.d.estr));
        } else if (!strcmp(actpblk.descr[i].name, "proxy.password")) {
            CHKiRet(assignParamFromEStr(&pData->proxyPassword, pvals[i].val.d.estr));
        } else {
            dbgprintf("omotlp: unhandled parameter '%s'\n", actpblk.descr[i].name);
        }
    }

    CHKiRet(applyEnvDefaults(pData));
    CHKiRet(ensureEndpointPathSplit(pData));

    /* Set default trace property names if not configured */
    if (pData->traceIdPropertyName == NULL) {
        CHKmalloc(pData->traceIdPropertyName = (uchar *)strdup("trace_id"));
    }
    if (pData->spanIdPropertyName == NULL) {
        CHKmalloc(pData->spanIdPropertyName = (uchar *)strdup("span_id"));
    }
    if (pData->traceFlagsPropertyName == NULL) {
        CHKmalloc(pData->traceFlagsPropertyName = (uchar *)strdup("trace_flags"));
    }

    if (pData->compression_mode == OMOTLP_COMPRESSION_UNSET) {
        pData->compression_mode = OMOTLP_COMPRESSION_NONE;
    }

    if (pData->compression_mode == OMOTLP_COMPRESSION_GZIP) {
        CHKiRet(header_list_add(&pData->headers, "Content-Encoding: gzip"));
    }

    if (pData->protocol == NULL) CHKmalloc(pData->protocol = (uchar *)strdup("http/json"));
    if (pData->endpoint == NULL) CHKmalloc(pData->endpoint = (uchar *)strdup("http://127.0.0.1:4318"));
    if (pData->path == NULL) CHKmalloc(pData->path = (uchar *)strdup("/v1/logs"));
    if (pData->bodyTemplateName == NULL) CHKmalloc(pData->bodyTemplateName = (uchar *)strdup("RSYSLOG_FileFormat"));

    lowercaseInPlace(pData->protocol);
    CHKiRet(validateProtocol(pData));
    CHKiRet(buildEffectiveUrl(pData));

    CODE_STD_STRING_REQUESTnewActInst(2);
    CHKiRet(OMSRsetEntry(*ppOMSR, OMOTLP_OMSR_IDX_MESSAGE, NULL, OMSR_TPL_AS_MSG));

    tplToUse = (uchar *)strdup((char *)pData->bodyTemplateName);
    if (tplToUse == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    CHKiRet(OMSRsetEntry(*ppOMSR, OMOTLP_OMSR_IDX_BODY, tplToUse, OMSR_NO_RQD_TPL_OPTS));

    CODE_STD_FINALIZERnewActInst;
    /* Free text allocations if they were allocated but not freed (error path cleanup) */
    if (text != NULL) {
        free(text);
        text = NULL;
    }
    /* Free bearer token allocations if they were allocated but not freed (error path cleanup) */
    /* On success path, these are already freed in the loop after header_list_add_kv succeeds */
    if (bearer_auth != NULL) {
        free(bearer_auth);
        bearer_auth = NULL;
    }
    if (bearer_token != NULL) {
        free(bearer_token);
        bearer_token = NULL;
    }
    /* Free JSON text and parsed object if they were allocated but not freed (error path cleanup) */
    if (resource_json_text != NULL) {
        free(resource_json_text);
        resource_json_text = NULL;
    }
    if (resource_parsed != NULL) {
        fjson_object_put(resource_parsed);
        resource_parsed = NULL;
    }
    if (attribute_map_json_text != NULL) {
        free(attribute_map_json_text);
        attribute_map_json_text = NULL;
    }
    if (severity_map_json_text != NULL) {
        free(severity_map_json_text);
        severity_map_json_text = NULL;
    }
    cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst

BEGINdoAction
    char *body = NULL;
    omotlp_log_record_t record = {0};
    long long now_ms;
    smsg_t **ppMsgParam = (smsg_t **)pMsgData;
    smsg_t *msg = (ppMsgParam != NULL) ? ppMsgParam[OMOTLP_OMSR_IDX_MESSAGE] : NULL;
    CODESTARTdoAction;

    if (pWrkrData->pData == NULL) {
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    if (pWrkrData->http_client == NULL) {
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    if (ppString != NULL && ppString[OMOTLP_OMSR_IDX_BODY] != NULL) {
        body = (char *)ppString[OMOTLP_OMSR_IDX_BODY];
    }

    if (body == NULL) {
        body = (char *)"";
    }

    now_ms = currentTimeMills();
    CHKiRet(omotlp_batch_flush_if_due(pWrkrData, now_ms));

    CHKiRet(populateLogRecord(msg, body, pWrkrData->pData, &record));
    CHKiRet(omotlp_batch_add_record(pWrkrData, &record, body));

    pthread_mutex_lock(&pWrkrData->batch_mutex);
    if (pWrkrData->batch.count == 0u) {
        iRet = RS_RET_OK;
    } else {
        iRet = RS_RET_DEFER_COMMIT;
    }
    pthread_mutex_unlock(&pWrkrData->batch_mutex);

finalize_it:
    /* Free trace correlation strings if they were allocated by populateLogRecord */
    if (record.trace_id != NULL) {
        free((char *)record.trace_id);
        record.trace_id = NULL;
    }
    if (record.span_id != NULL) {
        free((char *)record.span_id);
        record.span_id = NULL;
    }
ENDdoAction

NO_LEGACY_CONF_parseSelectorAct /* clang-format off */
BEGINmodExit
    CODESTARTmodExit;
    omotlp_http_global_cleanup();
    objRelease(datetime, CORE_COMPONENT);
    objRelease(prop, CORE_COMPONENT);
    objRelease(statsobj, CORE_COMPONENT);
ENDmodExit

BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
ENDisCompatibleWithFeature

BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMOD_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
ENDqueryEtryPt /* clang-format on */

BEGINmodInit()
    CODESTARTmodInit;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CHKiRet(omotlp_http_global_init());
    CHKiRet(objUse(datetime, CORE_COMPONENT));
    CHKiRet(objUse(prop, CORE_COMPONENT));
    CHKiRet(objUse(statsobj, CORE_COMPONENT));
ENDmodInit
