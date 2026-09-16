#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the helper fixtures and one real imtcp/default-parser/omfile path.
# Exact IDs after shutdown prove that RFC5424 MSG extraction reaches the sink;
# loose precision limits here test parser/oracle wiring, while benchmark runs
# use the stricter sub-millisecond invalidation limits. The trial is a fresh
# child testbench session so its diag.sh initialization cannot reuse this
# wrapper's generated-name state.
. ${srcdir:=.}/diag.sh init
command -v python3 >/dev/null 2>&1 || exit 77
python3 "$srcdir/../benchmarks/queue-contention/latency-observer-selftest.py" || error_exit 1
require_plugin imtcp
metric_file="$PWD/$RSYSLOG_DYNNAME.latency.json"
(
    unset RSYSLOG_DYNNAME TESTCONF_NM
    BENCH_RATE_DRIFT_PERCENT=100 BENCH_MESSAGES=4 BENCH_CONNECTIONS=1 BENCH_OFFERED_RATE=100 BENCH_PAYLOAD=128 \
        BENCH_MAX_LATENESS_US=100000 BENCH_MAX_POLL_GAP_US=100000 \
        BENCH_METRIC_FILE="$metric_file" \
        bash "$srcdir/../benchmarks/queue-contention/trial-latency.sh"
) || error_exit 1
exit_test
