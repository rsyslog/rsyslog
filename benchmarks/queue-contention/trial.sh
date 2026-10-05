#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH
# Full daemon lifecycle, with exact delivery checked outside the timed phase.
# Run from a configured build's tests directory using the standard testbench.
#
# This file is part of rsyslog.
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=${BENCH_MESSAGES:-100000}
: "${BENCH_QUEUE_SIZE:=32768}" "${BENCH_DEQUEUE_BATCH_SIZE:=1024}"
: "${BENCH_WORKER_MINIMUM:=1024}" "${BENCH_CONSUMER_WORKERS:=4}"
if (( BENCH_QUEUE_SIZE <= 0 || BENCH_DEQUEUE_BATCH_SIZE <= 0 || BENCH_WORKER_MINIMUM <= 0 || BENCH_CONSUMER_WORKERS <= 0 )); then
    echo "queue benchmark sizes and worker count must be positive" >&2
    exit 1
fi
generate_conf
add_conf '
global(processInternalMessages="off" abortOnUncleanConfig="on")
main_queue(queue.type="FixedArray" queue.size="'$BENCH_QUEUE_SIZE'"
    queue.workerThreads="'$BENCH_CONSUMER_WORKERS'" queue.workerThreadMinimumMessages="'$BENCH_WORKER_MINIMUM'"
    queue.dequeueBatchSize="'$BENCH_DEQUEUE_BATCH_SIZE'")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then {
    set $.parseStatus = parse_json("{\"nested\":{\"array\":[1,2,3,4,5,6,7,8],\"text\":\"queue lifecycle benchmark payload\"}}", "\$!payload");
    if ($.parseStatus == 0 and $!payload!nested!text == "queue lifecycle benchmark payload") then
        action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
start_ns=$(date +%s%N)
startup
injectmsg
shutdown_when_empty
wait_shutdown
end_ns=$(date +%s%N)
seq_check
printf '%s\n' "$((end_ns - start_ns))" > "$BENCH_METRIC_FILE"
exit_test
