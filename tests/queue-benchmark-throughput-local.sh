#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the local-scope throughput harness with a tiny imtcp workload. The
# real chkseq post-shutdown oracle in trial-multi proves that every expected ID
# reaches the qualified local omfile sink exactly once; this test makes no
# timing or throughput claim. The trial uses a fresh child testbench session so
# VPATH helper paths from this wrapper remain valid.

set -eo pipefail

: "${srcdir:=.}"
. "$srcdir/diag.sh" init
metric_file="$PWD/queue-benchmark-throughput-local-$$.json"
trap 'rm -f "$metric_file"' EXIT

(
    unset RSYSLOG_DYNNAME TESTCONF_NM
    BENCH_MESSAGES=8 BENCH_CONNECTIONS=1 BENCH_PAYLOAD=128 BENCH_INPUT_WORKERS=1 \
        BENCH_CONSUMER_WORKERS=2 BENCH_QUEUE_SIZE=1024 BENCH_DEQUEUE_BATCH_SIZE=16 \
        BENCH_WORKER_MINIMUM=1 BENCH_SCOPE=local BENCH_FRONTEND_SIZE=100 \
        BENCH_MAX_FRONTENDS=1 BENCH_METRIC_FILE="$metric_file" \
        bash "$srcdir/../benchmarks/queue-contention/trial-multi.sh"
) || error_exit 1

test -s "$metric_file"
exit_test
