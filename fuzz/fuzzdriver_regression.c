/* Verify both drivers fail before dispatch when initialization fails, and
 * dispatch a logical zero-length testcase through the shared parser/reset
 * path when initialization succeeds. Counters are the behavioral oracle. */

#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fuzzapi.h"
#include "fuzzdriver.h"

static rsRetVal init_result = RS_RET_OK;
static unsigned int parse_calls;
static unsigned int reset_calls;
static size_t last_parse_size;

rsRetVal rs_fuzz_init(void) {
    return init_result;
}

void rs_fuzz_reset_iteration(void) {
    ++reset_calls;
}

void rs_fuzz_parse_entry(const uint8_t *data, size_t len) {
    if (data == NULL) {
        fprintf(stderr, "empty input reached parser with a NULL buffer\n");
        exit(EXIT_FAILURE);
    }
    ++parse_calls;
    last_parse_size = len;
}

#define NOOP_ENTRY(name)                         \
    void name(const uint8_t *data, size_t len) { \
        (void)data;                              \
        (void)len;                               \
    }

NOOP_ENTRY(rs_fuzz_parse_rfc3164_entry)
NOOP_ENTRY(rs_fuzz_parse_rfc5424_entry)
NOOP_ENTRY(rs_fuzz_ruleset_entry)
NOOP_ENTRY(rs_fuzz_template_entry)
NOOP_ENTRY(rs_fuzz_template_json_entry)
NOOP_ENTRY(rs_fuzz_timestamp_entry)
NOOP_ENTRY(rs_fuzz_timestamp_format_entry)
NOOP_ENTRY(rs_fuzz_json_props_entry)
NOOP_ENTRY(rs_fuzz_json_accessor_entry)
NOOP_ENTRY(rs_fuzz_json_msgset_entry)
NOOP_ENTRY(rs_fuzz_structured_data_entry)
NOOP_ENTRY(rs_fuzz_template_dynamic_entry)
NOOP_ENTRY(rs_fuzz_tcp_framing_entry)
NOOP_ENTRY(rs_fuzz_queue_lifecycle_entry)
NOOP_ENTRY(rs_fuzz_imfile_line_entry)
NOOP_ENTRY(rs_fuzz_action_format_entry)
NOOP_ENTRY(rs_fuzz_cfsysline_entry)

int rs_fuzz_get_legacy_state(rs_fuzz_legacy_state_t *state) {
    (void)state;
    return -1;
}

static int make_empty_file(char *path, size_t path_size) {
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = "/tmp";
    }
    const int n = snprintf(path, path_size, "%s/rsyslog-fuzz-empty-XXXXXX", tmpdir);
    if (n < 0 || (size_t)n >= path_size) {
        return -1;
    }
    const int fd = mkstemp(path);
    if (fd < 0) {
        return -1;
    }
    return close(fd);
}

int main(void) {
    char empty_path[4096];
    char *file_argv[] = {(char *)"fuzz_rsyslog_parsers", empty_path, NULL};

    if (make_empty_file(empty_path, sizeof(empty_path)) != 0) {
        perror("could not create empty regression input");
        return EXIT_FAILURE;
    }

    init_result = RS_RET_ERR;
    if (rs_fuzz_file_main(2, file_argv) == 0 || parse_calls != 0 || reset_calls != 0) {
        fprintf(stderr, "file driver continued after initialization failure\n");
        (void)unlink(empty_path);
        return EXIT_FAILURE;
    }
    int argc = 0;
    char **argv = NULL;
    if (LLVMFuzzerInitialize(&argc, &argv) == 0 || parse_calls != 0 || reset_calls != 0) {
        fprintf(stderr, "libFuzzer driver accepted initialization failure\n");
        (void)unlink(empty_path);
        return EXIT_FAILURE;
    }

    init_result = RS_RET_OK;
    if (rs_fuzz_file_main(2, file_argv) != 0 || parse_calls != 1 || last_parse_size != 0 || reset_calls != 1) {
        fprintf(stderr, "file driver did not dispatch empty input through parser and reset\n");
        (void)unlink(empty_path);
        return EXIT_FAILURE;
    }
    if (LLVMFuzzerInitialize(&argc, &argv) != 0 || LLVMFuzzerTestOneInput(NULL, 0) != 0 || parse_calls != 2 ||
        last_parse_size != 0 || reset_calls != 2) {
        fprintf(stderr, "libFuzzer driver did not dispatch empty input through parser and reset\n");
        (void)unlink(empty_path);
        return EXIT_FAILURE;
    }

    if (unlink(empty_path) != 0) {
        perror("could not remove empty regression input");
        return EXIT_FAILURE;
    }
    puts("driver initialization and empty-input regressions: PASS");
    return EXIT_SUCCESS;
}
