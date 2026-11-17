/* SPDX-License-Identifier: Apache-2.0 */

/* Locate rsyslog's loadable runtime modules (lm*.so) for libFuzzer targets.
 *
 * The fuzz targets link the full rsyslogd, which dlopen()s its runtime
 * modules from RSYSLOG_MODDIR. Two layouts are supported:
 *
 *  - packaged: "<directory of the binary>/rsyslog-modules". OSS-Fuzz and
 *    similar runners ship only the fuzzer output directory, so the build
 *    script copies the modules next to the binaries.
 *  - build tree: "<top_builddir>/runtime/.libs", derived from the "/tools/"
 *    component of the binary path, or "../runtime/.libs" relative to it.
 *
 * Copyright 2026 Rainer Gerhards and Others
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
#ifndef INCLUDED_FUZZ_MODDIR_H
#define INCLUDED_FUZZ_MODDIR_H

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Set RSYSLOG_MODDIR for the binary named by argv0; aborts on failure. */
static void fuzzSetModuleDir(const char *const argv0) {
    static char executable_path[PATH_MAX];
    static char module_path[PATH_MAX];
    const char *slash;
    const char *tools_dir;
    int len;

    if (realpath(argv0, executable_path) == NULL) {
        len = snprintf(executable_path, sizeof(executable_path), "%s", argv0);
        if (len < 0 || (size_t)len >= sizeof(executable_path)) abort();
    }
    slash = strrchr(executable_path, '/');

    if (slash != NULL) {
        len = snprintf(module_path, sizeof(module_path), "%.*s/rsyslog-modules", (int)(slash - executable_path),
                       executable_path);
        if (len < 0 || (size_t)len >= sizeof(module_path)) abort();
        if (access(module_path, R_OK | X_OK) == 0) {
            if (setenv("RSYSLOG_MODDIR", module_path, 1) != 0) abort();
            return;
        }
    }

    tools_dir = strstr(executable_path, "/tools/");
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
}

#endif /* INCLUDED_FUZZ_MODDIR_H */
