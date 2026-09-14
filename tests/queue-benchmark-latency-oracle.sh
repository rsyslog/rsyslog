#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the helper fixtures and one real imtcp/default-parser/omfile path.
# Exact IDs after shutdown prove that RFC5424 MSG extraction reaches the sink;
# loose precision limits here test parser/oracle wiring, while benchmark runs
# use the stricter sub-millisecond invalidation limits.
. ${srcdir:=.}/diag.sh init
python3 "$srcdir/../benchmarks/queue-contention/latency-observer-selftest.py" || error_exit 1
require_plugin imtcp
BENCH_MESSAGES=4 BENCH_CONNECTIONS=1 BENCH_OFFERED_RATE=100 BENCH_PAYLOAD=128 \
BENCH_MAX_LATENESS_US=100000 BENCH_MAX_POLL_GAP_US=100000 \
BENCH_METRIC_FILE="$PWD/$RSYSLOG_DYNNAME.latency.json" \
bash "$srcdir/../benchmarks/queue-contention/trial-latency.sh" || error_exit 1
exit_test
