/* prometheus_remote_write.c
 * Minimal Prometheus Remote Write protobuf encoding
 *
 * Copyright 2024-2026 Adiscon GmbH and contributors
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

#ifdef ENABLE_IMPSTATS_PUSH

    #include <stdlib.h>
    #include <string.h>
    #include <sys/time.h>
    #include <snappy-c.h>

    #include "rsyslog.h"
    #include "prometheus_remote_write.h"
    #include "remote.pb-c.h"

/* Internal request structure */
struct prom_write_request_s {
    Prometheus__WriteRequest *pb_request;
    size_t n_timeseries;
    size_t capacity;
};

static void free_timeseries_array(Prometheus__TimeSeries **series, size_t n_series) {
    size_t i, j;

    if (series == NULL) return;

    for (i = 0; i < n_series; i++) {
        Prometheus__TimeSeries *ts = series[i];
        if (ts == NULL) continue;

        for (j = 0; j < ts->n_labels; j++) {
            if (ts->labels[j]) {
                free(ts->labels[j]->name);
                free(ts->labels[j]->value);
                free(ts->labels[j]);
            }
        }
        free(ts->labels);

        for (j = 0; j < ts->n_samples; j++) {
            free(ts->samples[j]);
        }
        free(ts->samples);

        free(ts);
    }
    free(series);
}

/* Comparison function for label sorting (lexicographic by name) */
static int label_cmp(const void *a, const void *b) {
    const Prometheus__Label *la = *(const Prometheus__Label **)a;
    const Prometheus__Label *lb = *(const Prometheus__Label **)b;
    return strcmp(la->name, lb->name);
}

prom_write_request_t *prom_write_request_new(void) {
    DEFiRet;
    prom_write_request_t *wr = NULL;
    CHKmalloc(wr = calloc(1, sizeof(prom_write_request_t)));

    CHKmalloc(wr->pb_request = calloc(1, sizeof(Prometheus__WriteRequest)));

    prometheus__write_request__init(wr->pb_request);
    wr->capacity = 128;
    CHKmalloc(wr->pb_request->timeseries = calloc(wr->capacity, sizeof(Prometheus__TimeSeries *)));

finalize_it:
    if (iRet != RS_RET_OK && wr) {
        if (wr->pb_request) free(wr->pb_request);
        free(wr);
        wr = NULL;
    }
    return wr;
}

prom_write_request_t *prom_write_request_new_with_series(Prometheus__TimeSeries **series, size_t n_series) {
    DEFiRet;
    prom_write_request_t *wr = NULL;

    CHKmalloc(wr = calloc(1, sizeof(prom_write_request_t)));
    CHKmalloc(wr->pb_request = calloc(1, sizeof(Prometheus__WriteRequest)));
    prometheus__write_request__init(wr->pb_request);

    wr->pb_request->timeseries = series;
    wr->pb_request->n_timeseries = n_series;
    wr->n_timeseries = n_series;
    wr->capacity = n_series;

finalize_it:
    if (iRet != RS_RET_OK && wr) {
        if (wr->pb_request) free(wr->pb_request);
        free(wr);
        wr = NULL;
    }
    return wr;
}

void prom_write_request_detach_series(prom_write_request_t *wr, Prometheus__TimeSeries ***series, size_t *n_series) {
    if (series == NULL || n_series == NULL || wr == NULL || wr->pb_request == NULL) {
        return;
    }

    *series = wr->pb_request->timeseries;
    *n_series = wr->n_timeseries;

    wr->pb_request->timeseries = NULL;
    wr->pb_request->n_timeseries = 0;
    wr->n_timeseries = 0;
    wr->capacity = 0;
}

size_t prom_write_request_get_packed_size(prom_write_request_t *wr) {
    if (wr == NULL || wr->pb_request == NULL) {
        return 0;
    }
    return prometheus__write_request__get_packed_size(wr->pb_request);
}

size_t prom_write_request_get_series_count(prom_write_request_t *wr) {
    return wr ? wr->n_timeseries : 0;
}

void prom_write_request_free_series(Prometheus__TimeSeries **series, size_t n_series) {
    free_timeseries_array(series, n_series);
}

rsRetVal prom_write_request_add_metric(prom_write_request_t *wr,
                                       const char *metric_name,
                                       double value,
                                       int64_t timestamp_ms,
                                       const char **label_pairs,
                                       size_t n_labels) {
    DEFiRet;
    Prometheus__TimeSeries *ts = NULL;
    Prometheus__Sample *sample = NULL;
    size_t i;
    sbool ts_added_to_request = 0;

    if (wr == NULL || metric_name == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    if (n_labels > 0 && label_pairs == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);

    /* Expand capacity if needed */
    if (wr->n_timeseries >= wr->capacity) {
        size_t new_capacity = wr->capacity * 2;
        Prometheus__TimeSeries **new_ts;
        CHKmalloc(new_ts = realloc(wr->pb_request->timeseries, new_capacity * sizeof(Prometheus__TimeSeries *)));
        wr->pb_request->timeseries = new_ts;
        wr->capacity = new_capacity;
    }

    /* Create TimeSeries */
    CHKmalloc(ts = calloc(1, sizeof(Prometheus__TimeSeries)));
    prometheus__time_series__init(ts);

    /* Create __name__ label */
    ts->n_labels = 1 + n_labels;
    CHKmalloc(ts->labels = calloc(ts->n_labels, sizeof(Prometheus__Label *)));

    /* Add __name__ label */
    CHKmalloc(ts->labels[0] = calloc(1, sizeof(Prometheus__Label)));
    prometheus__label__init(ts->labels[0]);
    CHKmalloc(ts->labels[0]->name = strdup("__name__"));
    CHKmalloc(ts->labels[0]->value = strdup(metric_name));

    /* Add additional labels */
    size_t labels_added = 1; /* Start at 1 for __name__ */
    for (i = 0; i < n_labels; i++) {
        /* Check for NULL label (safety) */
        if (label_pairs[i] == NULL) continue;

        const char *eq = strchr(label_pairs[i], '=');
        if (eq == NULL) continue; /* Skip malformed labels */

        CHKmalloc(ts->labels[labels_added] = calloc(1, sizeof(Prometheus__Label)));

        prometheus__label__init(ts->labels[labels_added]);

        size_t name_len = eq - label_pairs[i];
        CHKmalloc(ts->labels[labels_added]->name = strndup(label_pairs[i], name_len));
        CHKmalloc(ts->labels[labels_added]->value = strdup(eq + 1));
        labels_added++; /* Only increment if successfully added */
    }
    /* Update actual label count */
    ts->n_labels = labels_added;

    /* Create sample */
    CHKmalloc(sample = calloc(1, sizeof(Prometheus__Sample)));
    prometheus__sample__init(sample);

    sample->timestamp = timestamp_ms;
    sample->value = value;

    /* Allocate samples array */
    ts->n_samples = 1;
    CHKmalloc(ts->samples = calloc(1, sizeof(Prometheus__Sample *)));
    ts->samples[0] = sample;
    sample = NULL; /* Ownership transferred to ts->samples */

    /* Sort labels lexicographically by name (required by Prometheus spec) */
    if (ts->n_labels > 1) {
        qsort(ts->labels, ts->n_labels, sizeof(Prometheus__Label *), label_cmp);
    }

    /* Add to request */
    wr->pb_request->timeseries[wr->n_timeseries++] = ts;
    wr->pb_request->n_timeseries = wr->n_timeseries;
    ts_added_to_request = 1;

finalize_it:
    /* Cleanup on error: only if ts wasn't successfully added to request */
    if (iRet != RS_RET_OK) {
        /* Free sample if ownership wasn't transferred */
        if (sample != NULL) {
            free(sample);
        }
        if (ts != NULL && !ts_added_to_request) {
            /* Free partially constructed TimeSeries */
            if (ts->samples != NULL) {
                if (ts->samples[0] != NULL) {
                    free(ts->samples[0]);
                }
                free(ts->samples);
            }
            if (ts->labels != NULL) {
                for (i = 0; i < ts->n_labels; i++) {
                    if (ts->labels[i] != NULL) {
                        free(ts->labels[i]->name);
                        free(ts->labels[i]->value);
                        free(ts->labels[i]);
                    }
                }
                free(ts->labels);
            }
            free(ts);
        }
    }
    RETiRet;
}

rsRetVal prom_write_request_encode(prom_write_request_t *wr, uint8_t **out_data, size_t *out_size) {
    DEFiRet;
    size_t pb_size = 0;
    uint8_t *pb_data = NULL;
    size_t compressed_size = 0;
    uint8_t *compressed_data = NULL;
    snappy_status status;

    if (wr == NULL || out_data == NULL || out_size == NULL) {
        ABORT_FINALIZE(RS_RET_PARAM_ERROR);
    }

    /* Serialize protobuf */
    pb_size = prometheus__write_request__get_packed_size(wr->pb_request);
    CHKmalloc(pb_data = malloc(pb_size));

    prometheus__write_request__pack(wr->pb_request, pb_data);

    /* Compress with Snappy */
    compressed_size = snappy_max_compressed_length(pb_size);
    compressed_data = malloc(compressed_size);
    if (compressed_data == NULL) {
        free(pb_data);
        pb_data = NULL;
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    status = snappy_compress((const char *)pb_data, pb_size, (char *)compressed_data, &compressed_size);
    free(pb_data);
    pb_data = NULL; /* Prevent double-free in finalize_it */

    if (status != SNAPPY_OK) {
        free(compressed_data);
        LogError(0, RS_RET_ERR, "prometheus_remote_write: snappy_compress failed: %d", status);
        ABORT_FINALIZE(RS_RET_ERR);
    }

    *out_data = compressed_data;
    *out_size = compressed_size;

finalize_it:
    /* Cleanup on error */
    if (iRet != RS_RET_OK && pb_data != NULL) {
        free(pb_data);
    }
    RETiRet;
}

void prom_write_request_free(prom_write_request_t *wr) {
    if (wr == NULL) return;

    if (wr->pb_request) {
        free_timeseries_array(wr->pb_request->timeseries, wr->n_timeseries);
        free(wr->pb_request);
    }

    free(wr);
}

#endif /* ENABLE_IMPSTATS_PUSH */
