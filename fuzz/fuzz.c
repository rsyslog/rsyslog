/* SPDX-License-Identifier: Apache-2.0 */
/* fuzz.c
 * Shared standalone and AFL++ driver for the rsyslog fuzz targets.
 *
 * Copyright 2026 Rainer Gerhards and Others
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

#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fuzzapi.h"
#include "fuzzdriver.h"

#ifndef __AFL_LOOP
    #define __AFL_LOOP(_x) 0
#endif

#define MAX_INPUT_SIZE (1U << 20) /* 1 MiB cap to avoid giant allocations */

enum fuzz_target {
    TARGET_PARSE = 0,
    TARGET_PARSE_RFC3164,
    TARGET_PARSE_RFC5424,
    TARGET_RULESET,
    TARGET_TEMPLATE,
    TARGET_TEMPLATE_JSON,
    TARGET_TIMESTAMP,
    TARGET_JSON_PROPS,
    TARGET_JSON_ACCESSOR,
    TARGET_JSON_MSGSET,
    TARGET_STRUCTURED_DATA,
    TARGET_TCP_FRAMING,
    TARGET_QUEUE_LIFECYCLE,
    TARGET_IMFILE_LINE,
    TARGET_ACTION_FORMAT,
    TARGET_CFSYSLINE,
    TARGET_COUNT
};

typedef struct input_profile_s {
    int looks_json;
    int looks_5424;
    int looks_3164;
    int has_structured;
    int has_template;
    int has_timestamp;
    int has_cfg;
    int has_json_token;
} input_profile_t;

static int has_token(const uint8_t *d, size_t n, const char *tok) {
    if (d == NULL || tok == NULL) {
        return 0;
    }
    const size_t toklen = strlen(tok);
    if (toklen == 0 || toklen > n) {
        return 0;
    }
    return memmem(d, n, tok, toklen) != NULL;
}

static int has_char(const uint8_t *d, size_t n, char c) {
    if (d == NULL) {
        return 0;
    }
    for (size_t i = 0; i < n; ++i) {
        if (d[i] == (uint8_t)c) {
            return 1;
        }
    }
    return 0;
}

static int starts_with_digit(const uint8_t *d, size_t n) {
    if (d == NULL) {
        return 0;
    }
    return n > 0 && isdigit(d[0]);
}

static int looks_like_rfc3164(const uint8_t *d, size_t n) {
    if (n < 5) {
        return 0;
    }
    if (d[0] == '<') {
        size_t i = 1;
        while (i < n && isdigit(d[i])) {
            ++i;
        }
        if (i < n && d[i] == '>') {
            return 1;
        }
    }
    return 0;
}

static int looks_like_rfc5424(const uint8_t *d, size_t n) {
    if (n < 10) {
        return 0;
    }
    if (d[0] == '<') {
        size_t i = 1;
        while (i < n && isdigit(d[i])) {
            ++i;
        }
        if (i + 2 < n && d[i] == '>' && d[i + 1] == '1' && d[i + 2] == ' ') {
            return 1;
        }
    }
    return 0;
}

static int looks_like_json(const uint8_t *d, size_t n) {
    size_t i = 0;
    while (i < n && isspace(d[i])) {
        ++i;
    }
    if (i >= n) {
        return 0;
    }
    return d[i] == '{' || d[i] == '[';
}

static int looks_like_timestampish(const uint8_t *d, size_t n) {
    int digit_count = 0;
    for (size_t i = 0; i < n && i < 32; ++i) {
        if (isdigit(d[i])) {
            ++digit_count;
        }
    }
    return (digit_count > 6) || has_char(d, n, 'Z') || has_char(d, n, ':');
}

static input_profile_t analyze_input(const uint8_t *data, size_t size) {
    input_profile_t p = (input_profile_t){0};
    p.looks_json = looks_like_json(data, size);
    p.looks_5424 = looks_like_rfc5424(data, size);
    p.looks_3164 = looks_like_rfc3164(data, size);
    p.has_structured = has_char(data, size, '[') || has_char(data, size, '@') || has_char(data, size, ']') ||
                       has_token(data, size, "sdid=");
    p.has_template = has_char(data, size, '$') || has_char(data, size, '%') || has_char(data, size, 'T') ||
                     has_token(data, size, "%msg%") || has_token(data, size, "template(") ||
                     has_token(data, size, "call ");
    p.has_timestamp = looks_like_timestampish(data, size) || starts_with_digit(data, size) ||
                      has_char(data, size, '+') || has_char(data, size, '-');
    p.has_cfg = has_token(data, size, "mainmsg") || has_token(data, size, "debug") || has_token(data, size, "Action") ||
                has_token(data, size, "template") || has_token(data, size, "ruleset") ||
                has_token(data, size, "set ") || has_token(data, size, "input(");
    p.has_json_token = has_char(data, size, '{') || has_char(data, size, '}') || has_char(data, size, '[') ||
                       has_char(data, size, ']') || has_token(data, size, "json");
    return p;
}

static int read_input_file(const char *path, uint8_t **buf, size_t *len, size_t *cap) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }

    size_t target = MAX_INPUT_SIZE;
    if (fseek(fp, 0, SEEK_END) == 0) {
        long file_size = ftell(fp);
        if (file_size >= 0) {
            const size_t sz = (size_t)file_size;
            if (sz < target) {
                target = sz;
            }
        }
        if (fseek(fp, 0, SEEK_SET) != 0) {
            fclose(fp);
            return -1;
        }
    } else {
        clearerr(fp);
    }

    if (target == 0) {
        *len = 0;
        fclose(fp);
        return 0;
    }

    if (*cap < target) {
        const size_t new_cap = target;
        uint8_t *tmp = realloc(*buf, new_cap);
        if (tmp == NULL) {
            fclose(fp);
            return -1;
        }
        *buf = tmp;
        *cap = new_cap;
    }

    size_t bytes_read = 0;
    while (bytes_read < target) {
        const size_t chunk = fread(*buf + bytes_read, 1, target - bytes_read, fp);
        if (chunk == 0) {
            if (ferror(fp)) {
                fclose(fp);
                return -1;
            }
            break;
        }
        bytes_read += chunk;
        if (feof(fp)) {
            break;
        }
    }

    *len = bytes_read;
    fclose(fp);
    return 0;
}

static void fuzz_parse_target(const uint8_t *data, size_t size) {
    rs_fuzz_parse_entry(data, size);
}

static void fuzz_parse_rfc3164_target(const uint8_t *data, size_t size) {
    rs_fuzz_parse_rfc3164_entry(data, size);
}

static void fuzz_parse_rfc5424_target(const uint8_t *data, size_t size) {
    rs_fuzz_parse_rfc5424_entry(data, size);
}

static void fuzz_ruleset_target(const uint8_t *data, size_t size) {
    rs_fuzz_ruleset_entry(data, size);
}

static void fuzz_template_target(const uint8_t *data, size_t size) {
    rs_fuzz_template_entry(data, size);
}

static void fuzz_template_json_target(const uint8_t *data, size_t size) {
    rs_fuzz_template_json_entry(data, size);
}

static void fuzz_timestamp_target(const uint8_t *data, size_t size) {
    rs_fuzz_timestamp_entry(data, size);
}

static void fuzz_json_props_target(const uint8_t *data, size_t size) {
    rs_fuzz_json_props_entry(data, size);
}

static void fuzz_json_accessor_target(const uint8_t *data, size_t size) {
    rs_fuzz_json_accessor_entry(data, size);
}

static void fuzz_json_msgset_target(const uint8_t *data, size_t size) {
    rs_fuzz_json_msgset_entry(data, size);
}

static void fuzz_structured_data_target(const uint8_t *data, size_t size) {
    rs_fuzz_structured_data_entry(data, size);
}

static void fuzz_tcp_framing_target(const uint8_t *data, size_t size) {
    rs_fuzz_tcp_framing_entry(data, size);
}

static void fuzz_queue_lifecycle_target(const uint8_t *data, size_t size) {
    rs_fuzz_queue_lifecycle_entry(data, size);
}

static void fuzz_imfile_line_target(const uint8_t *data, size_t size) {
    rs_fuzz_imfile_line_entry(data, size);
}

static void fuzz_action_format_target(const uint8_t *data, size_t size) {
    rs_fuzz_action_format_entry(data, size);
}

static void fuzz_template_dynamic_target(const uint8_t *data, size_t size) {
    rs_fuzz_template_dynamic_entry(data, size);
}

static void fuzz_timestamp_format_target(const uint8_t *data, size_t size) {
    rs_fuzz_timestamp_format_entry(data, size);
}

static void fuzz_cfsysline_target(const uint8_t *data, size_t size) {
    rs_fuzz_cfsysline_entry(data, size);
}

static void run_target(enum fuzz_target target, const uint8_t *data, size_t size, unsigned char *seen) {
    if (target < 0 || target >= TARGET_COUNT) {
        return;
    }
    if (seen != NULL) {
        if (seen[target]) {
            return;
        }
        seen[target] = 1;
    }
    switch (target) {
        case TARGET_PARSE:
            fuzz_parse_target(data, size);
            break;
        case TARGET_PARSE_RFC3164:
            fuzz_parse_rfc3164_target(data, size);
            break;
        case TARGET_PARSE_RFC5424:
            fuzz_parse_rfc5424_target(data, size);
            break;
        case TARGET_RULESET:
            fuzz_ruleset_target(data, size);
            break;
        case TARGET_TEMPLATE:
            fuzz_template_target(data, size);
            break;
        case TARGET_TEMPLATE_JSON:
            fuzz_template_json_target(data, size);
            break;
        case TARGET_TIMESTAMP:
            fuzz_timestamp_target(data, size);
            fuzz_timestamp_format_target(data, size);
            break;
        case TARGET_JSON_PROPS:
            fuzz_json_props_target(data, size);
            break;
        case TARGET_JSON_ACCESSOR:
            fuzz_json_accessor_target(data, size);
            break;
        case TARGET_JSON_MSGSET:
            fuzz_json_msgset_target(data, size);
            break;
        case TARGET_STRUCTURED_DATA:
            fuzz_structured_data_target(data, size);
            break;
        case TARGET_TCP_FRAMING:
            fuzz_tcp_framing_target(data, size);
            break;
        case TARGET_QUEUE_LIFECYCLE:
            fuzz_queue_lifecycle_target(data, size);
            break;
        case TARGET_IMFILE_LINE:
            fuzz_imfile_line_target(data, size);
            break;
        case TARGET_ACTION_FORMAT:
            fuzz_action_format_target(data, size);
            break;
        case TARGET_CFSYSLINE:
            fuzz_cfsysline_target(data, size);
            break;
        case TARGET_COUNT:
            break;
        default:
            fuzz_parse_target(data, size);
            break;
    }
    if (target == TARGET_QUEUE_LIFECYCLE && seen != NULL) {
        /* Queue variants exercise global runtime state; keep later dispatches
         * from entering ruleset processing with stale per-action queue state. */
        seen[TARGET_RULESET] = 1;
    }
}

static void process_one_input(const uint8_t *data, size_t size) {
    static const uint8_t empty_input = 0;

    if (data == NULL) {
        if (size != 0) {
            return;
        }
        data = &empty_input;
    }
    if (size == 0) {
        /* Keep the logical input length at zero while still satisfying the
         * non-NULL buffer contract of the rsyslog message API. */
        unsigned char seen[TARGET_COUNT] = {0};
        run_target(TARGET_PARSE, data, 0, seen);
        return;
    }
    input_profile_t prof = analyze_input(data, size);
    unsigned char seen[TARGET_COUNT] = {0};

    run_target(TARGET_PARSE, data, size, seen);
    if (prof.looks_3164 || prof.looks_5424) {
        if (prof.looks_3164) {
            run_target(TARGET_PARSE_RFC3164, data, size, seen);
        }
        if (prof.looks_5424) {
            run_target(TARGET_PARSE_RFC5424, data, size, seen);
        }
        run_target(TARGET_RULESET, data, size, seen);
    }
    if (prof.looks_3164 || prof.looks_5424 || has_char(data, size, '\n')) {
        run_target(TARGET_TCP_FRAMING, data, size, seen);
    }
    if (has_char(data, size, '\n')) {
        run_target(TARGET_IMFILE_LINE, data, size, seen);
    }
    if (prof.has_structured) {
        run_target(TARGET_STRUCTURED_DATA, data, size, seen);
    }
    if (prof.looks_json || prof.has_json_token || prof.has_structured) {
        run_target(TARGET_JSON_PROPS, data, size, seen);
        run_target(TARGET_JSON_ACCESSOR, data, size, seen);
        run_target(TARGET_JSON_MSGSET, data, size, seen);
        run_target(TARGET_TEMPLATE_JSON, data, size, seen);
    }
    if (prof.has_template) {
        run_target(TARGET_TEMPLATE, data, size, seen);
        run_target(TARGET_TEMPLATE_JSON, data, size, seen);
        fuzz_template_dynamic_target(data, size);
        run_target(TARGET_ACTION_FORMAT, data, size, seen);
    }
    if (prof.has_cfg || has_token(data, size, "queue")) {
        run_target(TARGET_QUEUE_LIFECYCLE, data, size, seen);
    }
    if (prof.has_timestamp) {
        run_target(TARGET_TIMESTAMP, data, size, seen);
    }
    if (prof.has_cfg) {
        run_target(TARGET_CFSYSLINE, data, size, seen);
    }

    const uint8_t *payload = (size > 2) ? data + 1 : data;
    const size_t payload_size = (size > 2) ? size - 1 : size;

    const uint8_t selectors[] = {data[0], (uint8_t)(size), (size > 1) ? data[size / 2] : data[0],
                                 (size > 2) ? data[size - 1] : data[0]};
    for (size_t i = 0; i < sizeof(selectors) / sizeof(selectors[0]); ++i) {
        run_target((enum fuzz_target)(selectors[i] % TARGET_COUNT), payload, payload_size, seen);
    }

    /* deterministic extra dispatch based on input hash to keep coverage stable */
    uint32_t hash = (uint32_t)size;
    const size_t mix_len = size < 32 ? size : 32;
    for (size_t i = 0; i < mix_len; ++i) {
        hash = (hash * 16777619u) ^ data[i];
    }
    run_target((enum fuzz_target)(hash % TARGET_COUNT), payload, payload_size, seen);
}

void rs_fuzz_dispatch_input(const uint8_t *data, size_t size) {
    if (size > MAX_INPUT_SIZE) {
        return;
    }
    process_one_input(data, size);
    rs_fuzz_reset_iteration();
}

int rs_fuzz_file_main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file> [input_file ...]\n", argv[0]);
        return 1;
    }

    if (rs_fuzz_driver_init() != 0) {
        return 1;
    }

    uint8_t *buf = NULL;
    size_t cap = 0;
    size_t len = 0;

#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif

#ifdef __AFL_HAVE_MANUAL_CONTROL
    while (__AFL_LOOP(1000)) {
#else
    {
#endif
        for (int i = 1; i < argc; ++i) {
            if (read_input_file(argv[i], &buf, &len, &cap) != 0) {
                fprintf(stderr, "Error reading input '%s'\n", argv[i]);
                free(buf);
                return 1;
            }
            rs_fuzz_dispatch_input(buf, len);
        }
    }

    free(buf);
    return 0;
}

#ifndef RSYSLOG_FUZZ_NO_MAIN
int main(int argc, char **argv) {
    return rs_fuzz_file_main(argc, argv);
}
#endif
