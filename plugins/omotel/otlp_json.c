/**
 * @file otlp_json.c
 * @brief OTLP JSON payload builder implementation
 *
 * This file implements the OTLP/HTTP JSON payload builder. It converts
 * rsyslog log records into OpenTelemetry Protocol JSON format, handling
 * resource attributes, scope information, log records, and attribute mapping.
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

#include "otlp_json.h"

#include <json.h>
#include <stdlib.h>
#include <string.h>

#include "errmsg.h"

/* Forward declaration of attribute_map_t structure */
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

static rsRetVal add_attribute_entry(struct json_object *attributes,
                                    const char *key,
                                    struct json_object *value_json,
                                    const char *value_type_key) ATTR_NONNULL(1, 2, 3, 4);
static rsRetVal add_string_attribute(struct json_object *attributes, const char *key, const char *value)
    ATTR_NONNULL(1, 2);
static rsRetVal add_int_attribute(struct json_object *attributes, const char *key, int64_t value) ATTR_NONNULL(1, 2);
static rsRetVal add_double_attribute(struct json_object *attributes, const char *key, double value) ATTR_NONNULL(1, 2);
static rsRetVal add_bool_attribute(struct json_object *attributes, const char *key, int value) ATTR_NONNULL(1, 2);
static rsRetVal add_body(struct json_object *log_record, const char *body) ATTR_NONNULL(1);

static rsRetVal add_attribute_entry(struct json_object *attributes,
                                    const char *key,
                                    struct json_object *value_json,
                                    const char *value_type_key) {
    struct json_object *entry = NULL;
    struct json_object *value_wrapper = NULL;
    struct json_object *key_json = NULL;

    DEFiRet;

    entry = fjson_object_new_object();
    if (entry == NULL) {
        fjson_object_put(value_json);
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    value_wrapper = fjson_object_new_object();
    if (value_wrapper == NULL) {
        fjson_object_put(entry);
        fjson_object_put(value_json);
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    key_json = fjson_object_new_string(key);
    if (key_json == NULL) {
        fjson_object_put(value_wrapper);
        fjson_object_put(entry);
        fjson_object_put(value_json);
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    fjson_object_object_add(value_wrapper, value_type_key, value_json);
    fjson_object_object_add(entry, "key", key_json);
    fjson_object_object_add(entry, "value", value_wrapper);

    if (fjson_object_array_add(attributes, entry) != 0) {
        fjson_object_put(entry);
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

finalize_it:
    RETiRet;
}

static rsRetVal add_string_attribute(struct json_object *attributes, const char *key, const char *value) {
    struct json_object *string_json = NULL;

    DEFiRet;

    if (value == NULL || value[0] == '\0') {
        goto finalize_it;
    }

    string_json = fjson_object_new_string(value);
    if (string_json == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    CHKiRet(add_attribute_entry(attributes, key, string_json, "stringValue"));

finalize_it:
    RETiRet;
}

static rsRetVal add_int_attribute(struct json_object *attributes, const char *key, int64_t value) {
    struct json_object *int_json = NULL;

    DEFiRet;

    int_json = json_object_new_int64(value);
    if (int_json == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    CHKiRet(add_attribute_entry(attributes, key, int_json, "intValue"));

finalize_it:
    RETiRet;
}

static rsRetVal add_double_attribute(struct json_object *attributes, const char *key, double value) {
    struct json_object *double_json = NULL;

    DEFiRet;

    double_json = json_object_new_double(value);
    if (double_json == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    CHKiRet(add_attribute_entry(attributes, key, double_json, "doubleValue"));

finalize_it:
    RETiRet;
}

static rsRetVal add_bool_attribute(struct json_object *attributes, const char *key, int value) {
    struct json_object *bool_json = NULL;

    DEFiRet;

    bool_json = fjson_object_new_boolean(value);
    if (bool_json == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    CHKiRet(add_attribute_entry(attributes, key, bool_json, "boolValue"));

finalize_it:
    RETiRet;
}

static rsRetVal add_body(struct json_object *log_record, const char *body) {
    struct json_object *body_wrapper = NULL;
    struct json_object *string_json = NULL;

    DEFiRet;

    body_wrapper = fjson_object_new_object();
    if (body_wrapper == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    string_json = fjson_object_new_string(body != NULL ? body : "");
    if (string_json == NULL) {
        fjson_object_put(body_wrapper);
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    fjson_object_object_add(body_wrapper, "stringValue", string_json);
    fjson_object_object_add(log_record, "body", body_wrapper);

finalize_it:
    RETiRet;
}

rsRetVal omotel_json_build_export(const omotel_log_record_t *records,
                                  size_t record_count,
                                  const omotel_resource_attrs_t *resource_attrs,
                                  const attribute_map_t *attribute_map,
                                  char **out_payload) {
    struct json_object *root = NULL;
    struct json_object *resource_logs = NULL;
    struct json_object *resource_entry = NULL;
    struct json_object *resource = NULL;
    struct json_object *resource_attributes = NULL;
    struct json_object *scope_logs = NULL;
    struct json_object *scope_entry = NULL;
    struct json_object *scope = NULL;
    struct json_object *log_records = NULL;
    char *rendered = NULL;
    size_t i;

    DEFiRet;

    if (out_payload == NULL || records == NULL || record_count == 0) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    *out_payload = NULL;

    root = fjson_object_new_object();
    if (root == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    resource_logs = fjson_object_new_array();
    if (resource_logs == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    fjson_object_object_add(root, "resourceLogs", resource_logs);

    resource_entry = fjson_object_new_object();
    if (resource_entry == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    if (fjson_object_array_add(resource_logs, resource_entry) != 0) {
        fjson_object_put(resource_entry);
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    resource = fjson_object_new_object();
    if (resource == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    fjson_object_object_add(resource_entry, "resource", resource);

    resource_attributes = fjson_object_new_array();
    if (resource_attributes == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    fjson_object_object_add(resource, "attributes", resource_attributes);

    CHKiRet(add_string_attribute(resource_attributes, "service.name", "rsyslog"));
    CHKiRet(add_string_attribute(resource_attributes, "telemetry.sdk.name", "rsyslog-omotel"));
    CHKiRet(add_string_attribute(resource_attributes, "telemetry.sdk.language", "C"));
    CHKiRet(add_string_attribute(resource_attributes, "telemetry.sdk.version", VERSION));

    /* Add custom resource attributes from JSON configuration */
    if (resource_attrs != NULL && resource_attrs->custom_attributes != NULL) {
        struct json_object_iterator iter;
        struct json_object_iterator iter_end;

        iter = json_object_iter_begin(resource_attrs->custom_attributes);
        iter_end = json_object_iter_end(resource_attrs->custom_attributes);

        while (!json_object_iter_equal(&iter, &iter_end)) {
            const char *key = json_object_iter_peek_name(&iter);
            struct json_object *value_obj = json_object_iter_peek_value(&iter);
            const char *string_value = NULL;
            int64_t int_value = 0;
            double double_value = 0.0;
            int bool_value = 0;

            if (value_obj != NULL) {
                enum fjson_type value_type = fjson_object_get_type(value_obj);

                switch (value_type) {
                    case fjson_type_string:
                        string_value = fjson_object_get_string(value_obj);
                        CHKiRet(add_string_attribute(resource_attributes, key, string_value));
                        break;

                    case fjson_type_int:
                        int_value = fjson_object_get_int64(value_obj);
                        CHKiRet(add_int_attribute(resource_attributes, key, int_value));
                        break;

                    case fjson_type_double:
                        double_value = fjson_object_get_double(value_obj);
                        CHKiRet(add_double_attribute(resource_attributes, key, double_value));
                        break;

                    case fjson_type_boolean:
                        bool_value = fjson_object_get_boolean(value_obj);
                        CHKiRet(add_bool_attribute(resource_attributes, key, bool_value));
                        break;

                    case fjson_type_null:
                    case fjson_type_object:
                    case fjson_type_array:
                        /* Skip arrays, objects, null - OTLP resource attributes are flat key-value pairs */
                        break;

                    default:
                        /* Skip any other types */
                        break;
                }
            }

            json_object_iter_next(&iter);
        }
    }

    /* Legacy single-attribute support (backward compatibility) */
    if (resource_attrs != NULL) {
        if (resource_attrs->service_instance_id != NULL && resource_attrs->service_instance_id[0] != '\0') {
            CHKiRet(
                add_string_attribute(resource_attributes, "service.instance.id", resource_attrs->service_instance_id));
        }
        if (resource_attrs->deployment_environment != NULL && resource_attrs->deployment_environment[0] != '\0') {
            CHKiRet(add_string_attribute(resource_attributes, "deployment.environment",
                                         resource_attrs->deployment_environment));
        }
    }

    /* Set host.name at resource level only if all records share the same hostname.
     * This ensures correct attribution when batching messages from a single source.
     * If hostnames differ, hostname is only set per-record in attributes.
     */
    if (record_count > 0 && records[0].hostname != NULL && records[0].hostname[0] != '\0') {
        size_t j;
        int all_same_hostname = 1;
        const char *first_hostname = records[0].hostname;

        /* Validate all records have the same hostname */
        for (j = 1; j < record_count; ++j) {
            if (records[j].hostname == NULL || records[j].hostname[0] == '\0') {
                all_same_hostname = 0;
                break;
            }
            if (strcmp(records[j].hostname, first_hostname) != 0) {
                all_same_hostname = 0;
                break;
            }
        }

        /* Only set resource-level host.name if all records share the same hostname */
        if (all_same_hostname) {
            CHKiRet(add_string_attribute(resource_attributes, "host.name", first_hostname));
        }
    }

    scope_logs = fjson_object_new_array();
    if (scope_logs == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    fjson_object_object_add(resource_entry, "scopeLogs", scope_logs);

    scope_entry = fjson_object_new_object();
    if (scope_entry == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    if (fjson_object_array_add(scope_logs, scope_entry) != 0) {
        fjson_object_put(scope_entry);
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

    scope = fjson_object_new_object();
    if (scope == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    fjson_object_object_add(scope_entry, "scope", scope);
    fjson_object_object_add(scope, "name", fjson_object_new_string("rsyslog.omotel"));
    fjson_object_object_add(scope, "version", fjson_object_new_string(VERSION));

    log_records = fjson_object_new_array();
    if (log_records == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    fjson_object_object_add(scope_entry, "logRecords", log_records);

    for (i = 0; i < record_count; ++i) {
        const omotel_log_record_t *record = &records[i];
        struct json_object *log_record = NULL;
        struct json_object *attributes = NULL;

        log_record = fjson_object_new_object();
        if (log_record == NULL) {
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        if (fjson_object_array_add(log_records, log_record) != 0) {
            fjson_object_put(log_record);
            ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
        }

        fjson_object_object_add(log_record, "timeUnixNano", json_object_new_int64((long long)record->time_unix_nano));
        if (record->observed_time_unix_nano != 0) {
            fjson_object_object_add(log_record, "observedTimeUnixNano",
                                    json_object_new_int64((long long)record->observed_time_unix_nano));
        }
        fjson_object_object_add(log_record, "severityNumber", json_object_new_int((int)record->severity_number));
        if (record->severity_text != NULL) {
            fjson_object_object_add(log_record, "severityText", fjson_object_new_string(record->severity_text));
        }

        CHKiRet(add_body(log_record, record->body));

        if (record->trace_id != NULL) {
            fjson_object_object_add(log_record, "traceId", fjson_object_new_string(record->trace_id));
        }
        if (record->span_id != NULL) {
            fjson_object_object_add(log_record, "spanId", fjson_object_new_string(record->span_id));
        }
        if (record->trace_flags != 0) {
            fjson_object_object_add(log_record, "flags", json_object_new_int((int)record->trace_flags));
        }

        attributes = fjson_object_new_array();
        if (attributes == NULL) {
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        fjson_object_object_add(log_record, "attributes", attributes);

        /* Default syslog attributes */
        const char *hostname_attr = "log.syslog.hostname";
        const char *appname_attr = "log.syslog.appname";
        const char *procid_attr = "log.syslog.procid";
        const char *msgid_attr = "log.syslog.msgid";
        const char *facility_attr = "log.syslog.facility";

        /* Apply custom attribute mapping if configured */
        if (attribute_map != NULL) {
            const char *mapped;

            mapped = attribute_map_lookup(attribute_map, "hostname");
            if (mapped != NULL) {
                hostname_attr = mapped;
            }

            mapped = attribute_map_lookup(attribute_map, "appname");
            if (mapped != NULL) {
                appname_attr = mapped;
            }

            mapped = attribute_map_lookup(attribute_map, "procid");
            if (mapped != NULL) {
                procid_attr = mapped;
            }

            mapped = attribute_map_lookup(attribute_map, "msgid");
            if (mapped != NULL) {
                msgid_attr = mapped;
            }

            mapped = attribute_map_lookup(attribute_map, "facility");
            if (mapped != NULL) {
                facility_attr = mapped;
            }
        }

        CHKiRet(add_string_attribute(attributes, appname_attr, record->app_name));
        CHKiRet(add_string_attribute(attributes, procid_attr, record->proc_id));
        CHKiRet(add_string_attribute(attributes, msgid_attr, record->msg_id));
        CHKiRet(add_int_attribute(attributes, facility_attr, (int64_t)record->facility));
        if (record->hostname != NULL && record->hostname[0] != '\0') {
            CHKiRet(add_string_attribute(attributes, hostname_attr, record->hostname));
        }
    }

    rendered = strdup(fjson_object_to_json_string(root));
    if (rendered == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    *out_payload = rendered;

finalize_it:
    if (iRet != RS_RET_OK && rendered != NULL) {
        free(rendered);
    }
    if (root != NULL) {
        fjson_object_put(root);
    }
    RETiRet;
}
