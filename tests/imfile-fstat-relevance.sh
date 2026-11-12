#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Keep the fstat fault fixture's imfile consumer enabled when only its C helper
# changes. Check the real selector, an unrelated negative case and the explicit
# override. This is a cheap, platform-independent selector test: no daemon,
# preload library, file monitor or external service is needed.
set -eu

testdir="$(cd "${srcdir:-$(dirname "$0")}" && pwd)"

check_case() {
    local name="$1" changed="$2" forced="$3" expected="$4" status=0
    (
        export RSYSLOG_TESTBENCH_CHANGED_FILES="$changed"
        export RSYSLOG_TESTBENCH_FORCE_IMFILE_TESTS="$forced"
        unset RSYSLOG_TESTBENCH_FORCE_SERVICE_TESTS RSYSLOG_TESTBENCH_SKIP_SERVICE_RELEVANCE
        bash "$testdir/diag.sh" module-needs-testing imfile
    ) >/dev/null 2>&1 || status=$?
    if [ "$status" -ne "$expected" ]; then
        printf '%s: expected imfile relevance status %s, got %s\n' "$name" "$expected" "$status" >&2
        exit 1
    fi
}

check_case fstat-helper 'tests/override_fstat.c' 0 0
check_case imfile-module 'plugins/imfile/imfile.c' 0 0
check_case unrelated 'tests/rscript_ipv42num.sh' 0 1
check_case forced 'tests/rscript_ipv42num.sh' 1 0
