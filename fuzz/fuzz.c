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
    int digitCount = 0;
    for (size_t i = 0; i < n && i < 32; ++i) {
        if (isdigit(d[i])) {
            ++digitCount;
        }
    }
    return (digitCount > 6) || has_char(d, n, 'Z') || has_char(d, n, ':');
}

static input_profile_t analyze_input(const uint8_t *Data, size_t Size) {
    input_profile_t p = (input_profile_t){0};
    p.looks_json = looks_like_json(Data, Size);
    p.looks_5424 = looks_like_rfc5424(Data, Size);
    p.looks_3164 = looks_like_rfc3164(Data, Size);
    p.has_structured = has_char(Data, Size, '[') || has_char(Data, Size, '@') || has_char(Data, Size, ']') ||
                       has_token(Data, Size, "sdid=");
    p.has_template = has_char(Data, Size, '$') || has_char(Data, Size, '%') || has_char(Data, Size, 'T') ||
                     has_token(Data, Size, "%msg%") || has_token(Data, Size, "template(") ||
                     has_token(Data, Size, "call ");
    p.has_timestamp = looks_like_timestampish(Data, Size) || starts_with_digit(Data, Size) ||
                      has_char(Data, Size, '+') || has_char(Data, Size, '-');
    p.has_cfg = has_token(Data, Size, "mainmsg") || has_token(Data, Size, "debug") || has_token(Data, Size, "Action") ||
                has_token(Data, Size, "template") || has_token(Data, Size, "ruleset") ||
                has_token(Data, Size, "set ") || has_token(Data, Size, "input(");
    p.has_json_token = has_char(Data, Size, '{') || has_char(Data, Size, '}') || has_char(Data, Size, '[') ||
                       has_char(Data, Size, ']') || has_token(Data, Size, "json");
    return p;
}

static int read_input_file(const char *path, uint8_t **buf, size_t *len, size_t *cap) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }

    size_t target = MAX_INPUT_SIZE;
    if (fseek(fp, 0, SEEK_END) == 0) {
        long fileSize = ftell(fp);
        if (fileSize >= 0) {
            const size_t sz = (size_t)fileSize;
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
        const size_t newCap = target;
        uint8_t *tmp = realloc(*buf, newCap);
        if (tmp == NULL) {
            fclose(fp);
            return -1;
        }
        *buf = tmp;
        *cap = newCap;
    }

    size_t bytesRead = 0;
    while (bytesRead < target) {
        const size_t chunk = fread(*buf + bytesRead, 1, target - bytesRead, fp);
        if (chunk == 0) {
            if (ferror(fp)) {
                fclose(fp);
                return -1;
            }
            break;
        }
        bytesRead += chunk;
        if (feof(fp)) {
            break;
        }
    }

    *len = bytesRead;
    fclose(fp);
    return 0;
}

static void fuzz_parse_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_parse_entry(Data, Size);
}

static void fuzz_parse_rfc3164_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_parse_rfc3164_entry(Data, Size);
}

static void fuzz_parse_rfc5424_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_parse_rfc5424_entry(Data, Size);
}

static void fuzz_ruleset_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_ruleset_entry(Data, Size);
}

static void fuzz_template_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_template_entry(Data, Size);
}

static void fuzz_template_json_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_template_json_entry(Data, Size);
}

static void fuzz_timestamp_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_timestamp_entry(Data, Size);
}

static void fuzz_json_props_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_json_props_entry(Data, Size);
}

static void fuzz_json_accessor_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_json_accessor_entry(Data, Size);
}

static void fuzz_json_msgset_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_json_msgset_entry(Data, Size);
}

static void fuzz_structured_data_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_structured_data_entry(Data, Size);
}

static void fuzz_tcp_framing_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_tcp_framing_entry(Data, Size);
}

static void fuzz_queue_lifecycle_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_queue_lifecycle_entry(Data, Size);
}

static void fuzz_imfile_line_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_imfile_line_entry(Data, Size);
}

static void fuzz_action_format_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_action_format_entry(Data, Size);
}

static void fuzz_template_dynamic_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_template_dynamic_entry(Data, Size);
}

static void fuzz_timestamp_format_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_timestamp_format_entry(Data, Size);
}

static void fuzz_cfsysline_target(const uint8_t *Data, size_t Size) {
    rs_fuzz_cfsysline_entry(Data, Size);
}

static void run_target(enum fuzz_target target, const uint8_t *Data, size_t Size, unsigned char *seen) {
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
            fuzz_parse_target(Data, Size);
            break;
        case TARGET_PARSE_RFC3164:
            fuzz_parse_rfc3164_target(Data, Size);
            break;
        case TARGET_PARSE_RFC5424:
            fuzz_parse_rfc5424_target(Data, Size);
            break;
        case TARGET_RULESET:
            fuzz_ruleset_target(Data, Size);
            break;
        case TARGET_TEMPLATE:
            fuzz_template_target(Data, Size);
            break;
        case TARGET_TEMPLATE_JSON:
            fuzz_template_json_target(Data, Size);
            break;
        case TARGET_TIMESTAMP:
            fuzz_timestamp_target(Data, Size);
            fuzz_timestamp_format_target(Data, Size);
            break;
        case TARGET_JSON_PROPS:
            fuzz_json_props_target(Data, Size);
            break;
        case TARGET_JSON_ACCESSOR:
            fuzz_json_accessor_target(Data, Size);
            break;
        case TARGET_JSON_MSGSET:
            fuzz_json_msgset_target(Data, Size);
            break;
        case TARGET_STRUCTURED_DATA:
            fuzz_structured_data_target(Data, Size);
            break;
        case TARGET_TCP_FRAMING:
            fuzz_tcp_framing_target(Data, Size);
            break;
        case TARGET_QUEUE_LIFECYCLE:
            fuzz_queue_lifecycle_target(Data, Size);
            break;
        case TARGET_IMFILE_LINE:
            fuzz_imfile_line_target(Data, Size);
            break;
        case TARGET_ACTION_FORMAT:
            fuzz_action_format_target(Data, Size);
            break;
        case TARGET_CFSYSLINE:
            fuzz_cfsysline_target(Data, Size);
            break;
        case TARGET_COUNT:
            break;
        default:
            fuzz_parse_target(Data, Size);
            break;
    }
    if (target == TARGET_QUEUE_LIFECYCLE && seen != NULL) {
        /* Queue variants exercise global runtime state; keep later dispatches
         * from entering ruleset processing with stale per-action queue state. */
        seen[TARGET_RULESET] = 1;
    }
}

static void process_one_input(const uint8_t *Data, size_t Size) {
    static const uint8_t empty_input = 0;

    if (Data == NULL) {
        if (Size != 0) {
            return;
        }
        Data = &empty_input;
    }
    if (Size == 0) {
        /* Keep the logical input length at zero while still satisfying the
         * non-NULL buffer contract of the rsyslog message API. */
        unsigned char seen[TARGET_COUNT] = {0};
        run_target(TARGET_PARSE, Data, 0, seen);
        return;
    }
    input_profile_t prof = analyze_input(Data, Size);
    unsigned char seen[TARGET_COUNT] = {0};

    run_target(TARGET_PARSE, Data, Size, seen);
    if (prof.looks_3164 || prof.looks_5424) {
        if (prof.looks_3164) {
            run_target(TARGET_PARSE_RFC3164, Data, Size, seen);
        }
        if (prof.looks_5424) {
            run_target(TARGET_PARSE_RFC5424, Data, Size, seen);
        }
        run_target(TARGET_RULESET, Data, Size, seen);
    }
    if (prof.looks_3164 || prof.looks_5424 || has_char(Data, Size, '\n')) {
        run_target(TARGET_TCP_FRAMING, Data, Size, seen);
    }
    if (has_char(Data, Size, '\n')) {
        run_target(TARGET_IMFILE_LINE, Data, Size, seen);
    }
    if (prof.has_structured) {
        run_target(TARGET_STRUCTURED_DATA, Data, Size, seen);
    }
    if (prof.looks_json || prof.has_json_token || prof.has_structured) {
        run_target(TARGET_JSON_PROPS, Data, Size, seen);
        run_target(TARGET_JSON_ACCESSOR, Data, Size, seen);
        run_target(TARGET_JSON_MSGSET, Data, Size, seen);
        run_target(TARGET_TEMPLATE_JSON, Data, Size, seen);
    }
    if (prof.has_template) {
        run_target(TARGET_TEMPLATE, Data, Size, seen);
        run_target(TARGET_TEMPLATE_JSON, Data, Size, seen);
        fuzz_template_dynamic_target(Data, Size);
        run_target(TARGET_ACTION_FORMAT, Data, Size, seen);
    }
    if (prof.has_cfg || has_token(Data, Size, "queue")) {
        run_target(TARGET_QUEUE_LIFECYCLE, Data, Size, seen);
    }
    if (prof.has_timestamp) {
        run_target(TARGET_TIMESTAMP, Data, Size, seen);
    }
    if (prof.has_cfg) {
        run_target(TARGET_CFSYSLINE, Data, Size, seen);
    }

    const uint8_t *payload = (Size > 2) ? Data + 1 : Data;
    const size_t payloadSize = (Size > 2) ? Size - 1 : Size;

    const uint8_t selectors[] = {Data[0], (uint8_t)(Size), (Size > 1) ? Data[Size / 2] : Data[0],
                                 (Size > 2) ? Data[Size - 1] : Data[0]};
    for (size_t i = 0; i < sizeof(selectors) / sizeof(selectors[0]); ++i) {
        run_target((enum fuzz_target)(selectors[i] % TARGET_COUNT), payload, payloadSize, seen);
    }

    /* deterministic extra dispatch based on input hash to keep coverage stable */
    uint32_t hash = (uint32_t)Size;
    const size_t mixLen = Size < 32 ? Size : 32;
    for (size_t i = 0; i < mixLen; ++i) {
        hash = (hash * 16777619u) ^ Data[i];
    }
    run_target((enum fuzz_target)(hash % TARGET_COUNT), payload, payloadSize, seen);
}

void rs_fuzz_dispatch_input(const uint8_t *Data, size_t Size) {
    if (Size > MAX_INPUT_SIZE) {
        return;
    }
    process_one_input(Data, Size);
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
