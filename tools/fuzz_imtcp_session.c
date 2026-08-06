/*
 * Exercise the real imtcp session framing and decompression engine with
 * arbitrary byte streams and deterministic receive chunk boundaries.
 */
#include "config.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "obj.h"
#include "tcpsrv.h"
#include "tcps_sess.h"

DEFobjCurrIf(obj);

static void fuzzAbortOnError(const rsRetVal ret) {
    if (ret != RS_RET_OK) abort();
}

static void fuzzIgnoreExpectedError(const int severity __attribute__((unused)),
                                    const int error_number __attribute__((unused)),
                                    const uchar *message __attribute__((unused))) {}

static void fuzzCleanup(void) {
    tcps_sessFuzzExit();
    fuzzAbortOnError(rsrtExit());
}

int LLVMFuzzerInitialize(int *argc, char ***argv);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerInitialize(int *argc __attribute__((unused)), char ***argv) {
    const char *err_obj = "rsyslog runtime";
    const char *slash;
    const char *tools_dir;
    static char executable_path[PATH_MAX];
    static char module_path[PATH_MAX];
    int len;

    if (realpath((*argv)[0], executable_path) == NULL) {
        len = snprintf(executable_path, sizeof(executable_path), "%s", (*argv)[0]);
        if (len < 0 || (size_t)len >= sizeof(executable_path)) abort();
    }
    tools_dir = strstr(executable_path, "/tools/");
    slash = strrchr(executable_path, '/');
    if (tools_dir != NULL) {
        len = snprintf(module_path, sizeof(module_path), "%.*s/runtime/.libs", (int)(tools_dir - executable_path),
                       executable_path);
    } else if (slash == NULL) {
        len = snprintf(module_path, sizeof(module_path), "runtime/.libs");
    } else {
        len = snprintf(module_path, sizeof(module_path), "%.*s/../runtime/.libs", (int)(slash - executable_path),
                       executable_path);
    }
    if (len < 0 || (size_t)len >= sizeof(module_path) || setenv("RSYSLOG_MODDIR", module_path, 1) != 0) abort();

    fuzzAbortOnError(rsrtInit(&err_obj, &obj));
    fuzzAbortOnError(tcps_sessFuzzInit());
    rsrtSetErrLogger(fuzzIgnoreExpectedError);
    if (atexit(fuzzCleanup) != 0) abort();
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, const size_t size) {
    if (size > INT_MAX) return 0;
    fuzzAbortOnError(tcps_sessFuzzInput(data, size));
    return 0;
}
