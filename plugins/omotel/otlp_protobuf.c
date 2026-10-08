/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file otlp_protobuf.c
 * @brief OTLP protobuf payload builder implementation
 *
 * This file implements the OTLP/HTTP protobuf payload builder. It converts
 * rsyslog log records into an ExportLogsServiceRequest protobuf message
 * using the protobuf-c generated bindings.
 *
 * Copyright 2025-2026 Adiscon GmbH.
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

#include "otlp_protobuf.h"

#include <stdlib.h>
#include <string.h>

#include "errmsg.h"

#include "opentelemetry/proto/logs/v1/logs.pb-c.h"
#include "opentelemetry/proto/common/v1/common.pb-c.h"
#include "opentelemetry/proto/resource/v1/resource.pb-c.h"

/* Forward declaration of attribute_map_entry_s (matches otlp_json.c) */
struct attribute_map_entry_s {
    char *rsyslog_property;
    char *otlp_attribute;
};

struct attribute_map_s {
    struct attribute_map_entry_s *entries;
    size_t count;
    size_t capacity;
};

static const char *attribute_map_lookup(const attribute_map_t *map, const char *rsyslog_prop) {
    size_t i;

    if (map == NULL || rsyslog_prop == NULL) {
        return NULL;
    }

    for (i = 0; i < map->count; ++i) {
        if (strcmp(map->entries[i].rsyslog_property, rsyslog_prop) == 0) {
            return map->entries[i].otlp_attribute;
        }
    }

    return NULL;
}

/**
 * @brief Decode a hex character to its 4-bit value
 */
static int hex_to_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * @brief Decode a hex string to raw bytes
 *
 * @param[in] hex Hex string (must be exactly 2*out_len chars)
 * @param[out] out Output byte buffer
 * @param[in] out_len Expected output length in bytes
 * @return 0 on success, -1 on invalid input
 */
static int hex_decode(const char *hex, uint8_t *out, size_t out_len) {
    size_t i;
    for (i = 0; i < out_len; ++i) {
        int hi = hex_to_nibble(hex[2 * i]);
        int lo = hex_to_nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return 0;
}

/**
 * @brief Create a KeyValue attribute with a string value
 */
static Opentelemetry__Proto__Common__V1__KeyValue *make_string_kv(const char *key, const char *value) {
    Opentelemetry__Proto__Common__V1__KeyValue *kv;
    Opentelemetry__Proto__Common__V1__AnyValue *av;

    if (key == NULL || value == NULL) {
        return NULL;
    }

    kv = calloc(1, sizeof(*kv));
    if (kv == NULL) return NULL;
    opentelemetry__proto__common__v1__key_value__init(kv);

    kv->key = strdup(key);
    if (kv->key == NULL) {
        free(kv);
        return NULL;
    }

    av = calloc(1, sizeof(*av));
    if (av == NULL) {
        free(kv->key);
        free(kv);
        return NULL;
    }
    opentelemetry__proto__common__v1__any_value__init(av);
    av->value_case = OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_STRING_VALUE;
    av->string_value = strdup(value);
    if (av->string_value == NULL) {
        free(av);
        free(kv->key);
        free(kv);
        return NULL;
    }

    kv->value = av;
    return kv;
}

/**
 * @brief Create a KeyValue attribute with an int64 value
 */
static Opentelemetry__Proto__Common__V1__KeyValue *make_int_kv(const char *key, int64_t value) {
    Opentelemetry__Proto__Common__V1__KeyValue *kv;
    Opentelemetry__Proto__Common__V1__AnyValue *av;

    if (key == NULL) return NULL;

    kv = calloc(1, sizeof(*kv));
    if (kv == NULL) return NULL;
    opentelemetry__proto__common__v1__key_value__init(kv);

    kv->key = strdup(key);
    if (kv->key == NULL) {
        free(kv);
        return NULL;
    }

    av = calloc(1, sizeof(*av));
    if (av == NULL) {
        free(kv->key);
        free(kv);
        return NULL;
    }
    opentelemetry__proto__common__v1__any_value__init(av);
    av->value_case = OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_INT_VALUE;
    av->int_value = value;

    kv->value = av;
    return kv;
}

/**
 * @brief Create a KeyValue attribute with a double value
 */
static Opentelemetry__Proto__Common__V1__KeyValue *make_double_kv(const char *key, double value) {
    Opentelemetry__Proto__Common__V1__KeyValue *kv;
    Opentelemetry__Proto__Common__V1__AnyValue *av;

    if (key == NULL) return NULL;

    kv = calloc(1, sizeof(*kv));
    if (kv == NULL) return NULL;
    opentelemetry__proto__common__v1__key_value__init(kv);

    kv->key = strdup(key);
    if (kv->key == NULL) {
        free(kv);
        return NULL;
    }

    av = calloc(1, sizeof(*av));
    if (av == NULL) {
        free(kv->key);
        free(kv);
        return NULL;
    }
    opentelemetry__proto__common__v1__any_value__init(av);
    av->value_case = OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_DOUBLE_VALUE;
    av->double_value = value;

    kv->value = av;
    return kv;
}

/**
 * @brief Create a KeyValue attribute with a boolean value
 */
static Opentelemetry__Proto__Common__V1__KeyValue *make_bool_kv(const char *key, int value) {
    Opentelemetry__Proto__Common__V1__KeyValue *kv;
    Opentelemetry__Proto__Common__V1__AnyValue *av;

    if (key == NULL) return NULL;

    kv = calloc(1, sizeof(*kv));
    if (kv == NULL) return NULL;
    opentelemetry__proto__common__v1__key_value__init(kv);

    kv->key = strdup(key);
    if (kv->key == NULL) {
        free(kv);
        return NULL;
    }

    av = calloc(1, sizeof(*av));
    if (av == NULL) {
        free(kv->key);
        free(kv);
        return NULL;
    }
    opentelemetry__proto__common__v1__any_value__init(av);
    av->value_case = OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_BOOL_VALUE;
    av->bool_value = value;

    kv->value = av;
    return kv;
}

/**
 * @brief Free a KeyValue and its nested AnyValue
 */
static void free_kv(Opentelemetry__Proto__Common__V1__KeyValue *kv) {
    if (kv == NULL) return;
    if (kv->value != NULL) {
        if (kv->value->value_case == OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_STRING_VALUE) {
            free(kv->value->string_value);
        }
        free(kv->value);
    }
    free(kv->key);
    free(kv);
}

/**
 * @brief Growable array of KeyValue pointers for building attribute lists
 */
typedef struct {
    Opentelemetry__Proto__Common__V1__KeyValue **items;
    size_t count;
    size_t capacity;
} kv_array_t;

static rsRetVal kv_array_init(kv_array_t *arr, size_t initial_capacity) {
    DEFiRet;
    arr->count = 0;
    arr->capacity = initial_capacity;
    CHKmalloc(arr->items = calloc(initial_capacity, sizeof(*arr->items)));
finalize_it:
    RETiRet;
}

static rsRetVal kv_array_add(kv_array_t *arr, Opentelemetry__Proto__Common__V1__KeyValue *kv) {
    DEFiRet;
    if (kv == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    if (arr->count >= arr->capacity) {
        size_t new_cap = arr->capacity * 2;
        Opentelemetry__Proto__Common__V1__KeyValue **tmp;
        CHKmalloc(tmp = realloc(arr->items, new_cap * sizeof(*tmp)));
        arr->items = tmp;
        arr->capacity = new_cap;
    }
    arr->items[arr->count++] = kv;
finalize_it:
    if (iRet != RS_RET_OK) {
        free_kv(kv);
    }
    RETiRet;
}

/**
 * @brief Free all KV entries and the items array of an unlinked kv_array_t.
 *
 * Call only when ownership has NOT been transferred into the protobuf tree.
 */
static void kv_array_free(kv_array_t *arr) {
    size_t j;
    if (arr->items == NULL) return;
    for (j = 0; j < arr->count; ++j) {
        free_kv(arr->items[j]);
    }
    free(arr->items);
    arr->items = NULL;
    arr->count = 0;
}

/**
 * @brief Free a fully-constructed ExportLogsServiceRequest and all nested allocations
 */
static void free_export_request(Opentelemetry__Proto__Logs__V1__ExportLogsServiceRequest *req) {
    size_t rl_idx, sl_idx, lr_idx, a_idx;

    if (req == NULL) return;

    for (rl_idx = 0; rl_idx < req->n_resource_logs; ++rl_idx) {
        Opentelemetry__Proto__Logs__V1__ResourceLogs *rl = req->resource_logs[rl_idx];
        if (rl == NULL) continue;

        /* Free resource attributes */
        if (rl->resource != NULL) {
            for (a_idx = 0; a_idx < rl->resource->n_attributes; ++a_idx) {
                free_kv(rl->resource->attributes[a_idx]);
            }
            free(rl->resource->attributes);
            free(rl->resource);
        }

        for (sl_idx = 0; sl_idx < rl->n_scope_logs; ++sl_idx) {
            Opentelemetry__Proto__Logs__V1__ScopeLogs *sl = rl->scope_logs[sl_idx];
            if (sl == NULL) continue;

            /* Free scope */
            if (sl->scope != NULL) {
                free(sl->scope->name);
                free(sl->scope->version);
                free(sl->scope);
            }

            for (lr_idx = 0; lr_idx < sl->n_log_records; ++lr_idx) {
                Opentelemetry__Proto__Logs__V1__LogRecord *lr = sl->log_records[lr_idx];
                if (lr == NULL) continue;

                /* Free body */
                if (lr->body != NULL) {
                    if (lr->body->value_case == OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_STRING_VALUE) {
                        free(lr->body->string_value);
                    }
                    free(lr->body);
                }

                /* Free severity_text */
                if (lr->severity_text != NULL && lr->severity_text != (char *)protobuf_c_empty_string) {
                    free(lr->severity_text);
                }

                /* Free attributes */
                for (a_idx = 0; a_idx < lr->n_attributes; ++a_idx) {
                    free_kv(lr->attributes[a_idx]);
                }
                free(lr->attributes);

                /* Free trace_id / span_id binary data */
                free(lr->trace_id.data);
                free(lr->span_id.data);

                free(lr);
            }
            free(sl->log_records);
            free(sl);
        }
        /* scope_logs array is stack-allocated by caller; do not free */
        free(rl);
    }
    /* resource_logs array is stack-allocated by caller; do not free */
}

rsRetVal omotel_protobuf_build_export(const omotel_log_record_t *records,
                                      size_t record_count,
                                      const omotel_resource_attrs_t *resource_attrs,
                                      const attribute_map_t *attribute_map,
                                      uint8_t **out_payload,
                                      size_t *out_len) {
    Opentelemetry__Proto__Logs__V1__ExportLogsServiceRequest request =
        OPENTELEMETRY__PROTO__LOGS__V1__EXPORT_LOGS_SERVICE_REQUEST__INIT;
    Opentelemetry__Proto__Logs__V1__ResourceLogs *resource_log = NULL;
    Opentelemetry__Proto__Logs__V1__ResourceLogs *resource_logs_arr[1];
    Opentelemetry__Proto__Resource__V1__Resource *resource = NULL;
    Opentelemetry__Proto__Logs__V1__ScopeLogs *scope_log = NULL;
    Opentelemetry__Proto__Logs__V1__ScopeLogs *scope_logs_arr[1];
    Opentelemetry__Proto__Common__V1__InstrumentationScope *scope = NULL;
    Opentelemetry__Proto__Logs__V1__LogRecord **pb_records = NULL;
    Opentelemetry__Proto__Common__V1__KeyValue *kv = NULL;
    kv_array_t resource_kvs = {0};
    kv_array_t rec_attrs = {0};
    size_t packed_size = 0;
    uint8_t *packed = NULL;
    size_t i;
    int need_cleanup = 0;

    DEFiRet;

    if (out_payload == NULL || out_len == NULL || records == NULL || record_count == 0) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    *out_payload = NULL;
    *out_len = 0;
    need_cleanup = 1;

    /* --- Create container skeleton and link into request early ---
     * This ensures free_export_request() can walk the partially-built
     * tree on any error path, preventing memory leaks. */
    CHKmalloc(resource_log = calloc(1, sizeof(*resource_log)));
    opentelemetry__proto__logs__v1__resource_logs__init(resource_log);
    resource_logs_arr[0] = resource_log;
    request.resource_logs = resource_logs_arr;
    request.n_resource_logs = 1;

    CHKmalloc(scope_log = calloc(1, sizeof(*scope_log)));
    opentelemetry__proto__logs__v1__scope_logs__init(scope_log);
    scope_logs_arr[0] = scope_log;
    resource_log->scope_logs = scope_logs_arr;
    resource_log->n_scope_logs = 1;

    /* --- Build resource attributes --- */
    CHKiRet(kv_array_init(&resource_kvs, 8));
    CHKmalloc(kv = make_string_kv("service.name", "rsyslog"));
    CHKiRet(kv_array_add(&resource_kvs, kv));
    CHKmalloc(kv = make_string_kv("telemetry.sdk.name", "rsyslog-omotel"));
    CHKiRet(kv_array_add(&resource_kvs, kv));
    CHKmalloc(kv = make_string_kv("telemetry.sdk.language", "C"));
    CHKiRet(kv_array_add(&resource_kvs, kv));
    CHKmalloc(kv = make_string_kv("telemetry.sdk.version", VERSION));
    CHKiRet(kv_array_add(&resource_kvs, kv));

    /* Custom resource attributes from JSON configuration */
    if (resource_attrs != NULL && resource_attrs->custom_attributes != NULL) {
        struct json_object_iterator iter = json_object_iter_begin(resource_attrs->custom_attributes);
        struct json_object_iterator iter_end = json_object_iter_end(resource_attrs->custom_attributes);

        while (!json_object_iter_equal(&iter, &iter_end)) {
            const char *key = json_object_iter_peek_name(&iter);
            struct json_object *val = json_object_iter_peek_value(&iter);

            if (val != NULL) {
                enum fjson_type vtype = fjson_object_get_type(val);
                switch (vtype) {
                    case fjson_type_string:
                        if (fjson_object_get_string(val) != NULL && fjson_object_get_string(val)[0] != '\0') {
                            CHKmalloc(kv = make_string_kv(key, fjson_object_get_string(val)));
                            CHKiRet(kv_array_add(&resource_kvs, kv));
                        }
                        break;
                    case fjson_type_int:
                        CHKmalloc(kv = make_int_kv(key, fjson_object_get_int64(val)));
                        CHKiRet(kv_array_add(&resource_kvs, kv));
                        break;
                    case fjson_type_double:
                        CHKmalloc(kv = make_double_kv(key, fjson_object_get_double(val)));
                        CHKiRet(kv_array_add(&resource_kvs, kv));
                        break;
                    case fjson_type_boolean:
                        CHKmalloc(kv = make_bool_kv(key, fjson_object_get_boolean(val)));
                        CHKiRet(kv_array_add(&resource_kvs, kv));
                        break;
                    case fjson_type_null:
                    case fjson_type_object:
                    case fjson_type_array:
                    default:
                        break;
                }
            }
            json_object_iter_next(&iter);
        }
    }

    /* Legacy single-attribute support */
    if (resource_attrs != NULL) {
        if (resource_attrs->service_instance_id != NULL && resource_attrs->service_instance_id[0] != '\0') {
            CHKmalloc(kv = make_string_kv("service.instance.id", resource_attrs->service_instance_id));
            CHKiRet(kv_array_add(&resource_kvs, kv));
        }
        if (resource_attrs->deployment_environment != NULL && resource_attrs->deployment_environment[0] != '\0') {
            CHKmalloc(kv = make_string_kv("deployment.environment", resource_attrs->deployment_environment));
            CHKiRet(kv_array_add(&resource_kvs, kv));
        }
    }

    /* Set host.name at resource level if all records share the same hostname */
    if (record_count > 0 && records[0].hostname != NULL && records[0].hostname[0] != '\0') {
        int all_same = 1;
        for (i = 1; i < record_count; ++i) {
            if (records[i].hostname == NULL || records[i].hostname[0] == '\0' ||
                strcmp(records[i].hostname, records[0].hostname) != 0) {
                all_same = 0;
                break;
            }
        }
        if (all_same) {
            CHKmalloc(kv = make_string_kv("host.name", records[0].hostname));
            CHKiRet(kv_array_add(&resource_kvs, kv));
        }
    }

    /* --- Build Resource and link to resource_log --- */
    CHKmalloc(resource = calloc(1, sizeof(*resource)));
    opentelemetry__proto__resource__v1__resource__init(resource);
    resource->n_attributes = resource_kvs.count;
    resource->attributes = resource_kvs.items;
    resource_kvs.items = NULL; /* ownership transferred to resource */
    resource_log->resource = resource;

    /* --- Build LogRecords (linked incrementally to scope_log) --- */
    CHKmalloc(pb_records = calloc(record_count, sizeof(*pb_records)));
    scope_log->log_records = pb_records;
    scope_log->n_log_records = 0;

    for (i = 0; i < record_count; ++i) {
        const omotel_log_record_t *rec = &records[i];
        Opentelemetry__Proto__Logs__V1__LogRecord *lr;

        CHKmalloc(lr = calloc(1, sizeof(*lr)));
        opentelemetry__proto__logs__v1__log_record__init(lr);
        pb_records[i] = lr;

        lr->time_unix_nano = rec->time_unix_nano;
        lr->observed_time_unix_nano = rec->observed_time_unix_nano;
        lr->severity_number = (Opentelemetry__Proto__Logs__V1__SeverityNumber)rec->severity_number;

        if (rec->severity_text != NULL) {
            lr->severity_text = strdup(rec->severity_text);
            if (lr->severity_text == NULL) {
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
        }

        /* Body as AnyValue(string) */
        {
            Opentelemetry__Proto__Common__V1__AnyValue *body_av;
            CHKmalloc(body_av = calloc(1, sizeof(*body_av)));
            opentelemetry__proto__common__v1__any_value__init(body_av);
            body_av->value_case = OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_STRING_VALUE;
            body_av->string_value = strdup(rec->body != NULL ? rec->body : "");
            if (body_av->string_value == NULL) {
                free(body_av);
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            lr->body = body_av;
        }

        /* Trace correlation: decode hex strings to raw bytes */
        if (rec->trace_id != NULL && strlen(rec->trace_id) == 32) {
            lr->trace_id.len = 16;
            CHKmalloc(lr->trace_id.data = malloc(16));
            if (hex_decode(rec->trace_id, lr->trace_id.data, 16) != 0) {
                free(lr->trace_id.data);
                lr->trace_id.data = NULL;
                lr->trace_id.len = 0;
            }
        }
        if (rec->span_id != NULL && strlen(rec->span_id) == 16) {
            lr->span_id.len = 8;
            CHKmalloc(lr->span_id.data = malloc(8));
            if (hex_decode(rec->span_id, lr->span_id.data, 8) != 0) {
                free(lr->span_id.data);
                lr->span_id.data = NULL;
                lr->span_id.len = 0;
            }
        }
        lr->flags = (uint32_t)rec->trace_flags;

        /* Build per-record attributes */
        CHKiRet(kv_array_init(&rec_attrs, 8));

        {
            const char *hostname_attr = "log.syslog.hostname";
            const char *appname_attr = "log.syslog.appname";
            const char *procid_attr = "log.syslog.procid";
            const char *msgid_attr = "log.syslog.msgid";
            const char *facility_attr = "log.syslog.facility";
            const char *mapped;

            if (attribute_map != NULL) {
                mapped = attribute_map_lookup(attribute_map, "hostname");
                if (mapped != NULL) hostname_attr = mapped;
                mapped = attribute_map_lookup(attribute_map, "appname");
                if (mapped != NULL) appname_attr = mapped;
                mapped = attribute_map_lookup(attribute_map, "procid");
                if (mapped != NULL) procid_attr = mapped;
                mapped = attribute_map_lookup(attribute_map, "msgid");
                if (mapped != NULL) msgid_attr = mapped;
                mapped = attribute_map_lookup(attribute_map, "facility");
                if (mapped != NULL) facility_attr = mapped;
            }

            if (rec->app_name != NULL && rec->app_name[0] != '\0') {
                CHKmalloc(kv = make_string_kv(appname_attr, rec->app_name));
                CHKiRet(kv_array_add(&rec_attrs, kv));
            }
            if (rec->proc_id != NULL && rec->proc_id[0] != '\0') {
                CHKmalloc(kv = make_string_kv(procid_attr, rec->proc_id));
                CHKiRet(kv_array_add(&rec_attrs, kv));
            }
            if (rec->msg_id != NULL && rec->msg_id[0] != '\0') {
                CHKmalloc(kv = make_string_kv(msgid_attr, rec->msg_id));
                CHKiRet(kv_array_add(&rec_attrs, kv));
            }
            CHKmalloc(kv = make_int_kv(facility_attr, (int64_t)rec->facility));
            CHKiRet(kv_array_add(&rec_attrs, kv));
            if (rec->hostname != NULL && rec->hostname[0] != '\0') {
                CHKmalloc(kv = make_string_kv(hostname_attr, rec->hostname));
                CHKiRet(kv_array_add(&rec_attrs, kv));
            }
        }

        lr->n_attributes = rec_attrs.count;
        lr->attributes = rec_attrs.items;
        rec_attrs.items = NULL; /* ownership transferred to lr */
        rec_attrs.count = 0;
        scope_log->n_log_records = i + 1; /* update count so cleanup can walk */
    }

    /* --- Build InstrumentationScope and link to scope_log --- */
    CHKmalloc(scope = calloc(1, sizeof(*scope)));
    opentelemetry__proto__common__v1__instrumentation_scope__init(scope);
    scope->name = strdup("rsyslog.omotel");
    scope->version = strdup(VERSION);
    if (scope->name == NULL || scope->version == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    scope_log->scope = scope;

    /* --- Serialize --- */
    packed_size = opentelemetry__proto__logs__v1__export_logs_service_request__get_packed_size(&request);
    CHKmalloc(packed = malloc(packed_size));
    opentelemetry__proto__logs__v1__export_logs_service_request__pack(&request, packed);

    *out_payload = packed;
    *out_len = packed_size;
    packed = NULL; /* ownership transferred */

finalize_it:
    if (need_cleanup) {
        /* Free the protobuf tree (but not the packed output) */
        free_export_request(&request);
        /* Free kv arrays whose ownership was not transferred */
        kv_array_free(&resource_kvs);
        kv_array_free(&rec_attrs);
    }
    if (iRet != RS_RET_OK) {
        free(packed);
    }
    RETiRet;
}
