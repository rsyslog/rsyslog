#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH
# Eight imtcp workers consume concurrent TCP connections and submit to a
# four/eight-worker FixedArray main queue. tcpflood -Y uses one sending thread
# per connection and disjoint message-ID ranges; its supervised completion and
# exact final IDs prove the generators and all daemon workers completed.
# Timing separates startup/shutdown from generation-plus-receiver-line-barrier
# work. Exact ID verification intentionally follows clean shutdown and is
# recorded separately, so sorting is not folded into the throughput metric.
# The output line count is the receiver barrier; no fixed sleep participates in
# the oracle.
#
# This file is part of rsyslog.
# Released under ASL 2.0
export NUMMESSAGES=${BENCH_MESSAGES:-1000000}
: "${BENCH_INPUT_WORKERS:=8}" "${BENCH_CONSUMER_WORKERS:=4}"
: "${BENCH_CONNECTIONS:=16}" "${BENCH_PAYLOAD:=512}"
: "${BENCH_QUEUE_SIZE:=32768}" "${BENCH_DEQUEUE_BATCH_SIZE:=1024}"
: "${BENCH_WORKER_MINIMUM:=1024}" "${BENCH_PRODUCER_MODE:=balanced}"
: "${BENCH_IMPSTATS:=no}" "${BENCH_IMPSTATS_FILE:=}"
if (( BENCH_CONNECTIONS <= 0 )); then
    echo "BENCH_CONNECTIONS must be positive" >&2
    exit 1
fi
if (( BENCH_QUEUE_SIZE <= 0 || BENCH_DEQUEUE_BATCH_SIZE <= 0 || BENCH_WORKER_MINIMUM <= 0 )); then
    echo "queue size, dequeue batch size, and worker minimum must be positive" >&2
    exit 1
fi
case "$BENCH_PRODUCER_MODE" in
balanced)
    BENCH_ACTIVE_CONNECTIONS=$BENCH_CONNECTIONS
    ;;
skew)
    # Keep input and consumer worker budgets fixed while only one TCP connection
    # carries the finite burst. imtcp schedules that connection dynamically, so
    # this is a connection-skew control, not proof of a hot producer identity.
    BENCH_ACTIVE_CONNECTIONS=1
    ;;
*)
    echo "BENCH_PRODUCER_MODE must be balanced or skew" >&2
    exit 1
    ;;
esac
case "$BENCH_IMPSTATS" in
yes)
    if [[ -z "$BENCH_IMPSTATS_FILE" ]]; then
        echo "BENCH_IMPSTATS_FILE is required when BENCH_IMPSTATS=yes" >&2
        exit 1
    fi
    BENCH_MUTEX_CONTENTION_STATS=on
    ;;
no)
    BENCH_MUTEX_CONTENTION_STATS=off
    ;;
*)
    echo "BENCH_IMPSTATS must be yes or no" >&2
    exit 1
    ;;
esac
if (( NUMMESSAGES % BENCH_ACTIVE_CONNECTIONS != 0 )); then
    echo "BENCH_MESSAGES ($NUMMESSAGES) must be divisible by active connections ($BENCH_ACTIVE_CONNECTIONS)" >&2
    exit 1
fi
. ${srcdir:=.}/diag.sh init
PORT_FILE="$PWD/$RSYSLOG_DYNNAME.input.port"
STATS_FILE="$PWD/$RSYSLOG_DYNNAME.queue.impstats.json"
generate_conf
if [[ "$BENCH_IMPSTATS" == yes ]]; then
    add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATS_FILE'" interval="1" format="json" log.syslog="off")
'
fi
add_conf '
global(processInternalMessages="off" abortOnUncleanConfig="on")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
    listenPortFileName="'$PORT_FILE'" workerThreads="'$BENCH_INPUT_WORKERS'")
main_queue(queue.type="FixedArray" queue.size="'$BENCH_QUEUE_SIZE'"
    queue.workerThreads="'$BENCH_CONSUMER_WORKERS'" queue.workerThreadMinimumMessages="'$BENCH_WORKER_MINIMUM'"
    queue.dequeueBatchSize="'$BENCH_DEQUEUE_BATCH_SIZE'" queue.mutexContentionStats="'$BENCH_MUTEX_CONTENTION_STATS'")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then {
    set $.parseStatus = parse_json("{\"nested\":{\"array\":[1,2,3,4,5,6,7,8],\"text\":\"queue contention benchmark payload\"}}", "\$!payload");
    if ($.parseStatus == 0 and $!payload!nested!text == "queue contention benchmark payload") then
        action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
start_ns=$(date +%s%N)
startup
assign_file_content INPUT_PORT "$PORT_FILE"
work_start_ns=$(date +%s%N)
tcpflood -p"$INPUT_PORT" -c"$BENCH_ACTIVE_CONNECTIONS" -Y -m"$NUMMESSAGES" -d"$BENCH_PAYLOAD" >/dev/null
generator_end_ns=$(date +%s%N)
wait_file_lines --delay 10 --abort-on-oversize "$RSYSLOG_OUT_LOG" "$NUMMESSAGES" 120
receiver_line_barrier_ns=$(date +%s%N)
shutdown_when_empty
wait_shutdown
shutdown_end_ns=$(date +%s%N)
if [[ "$BENCH_IMPSTATS" == yes ]]; then
    if [[ ! -s "$STATS_FILE" ]]; then
        echo "impstats file is missing or empty: $STATS_FILE" >&2
        error_exit 1
    fi
    cp "$STATS_FILE" "$BENCH_IMPSTATS_FILE"
fi
verify_start_ns=$(date +%s%N)
seq_check
verify_end_ns=$(date +%s%N)
printf '{"lifecycle_ns":%d,"work_ns":%d,"generator_ns":%d,"drain_ns":%d,"receiver_line_barrier_ns":%d,"post_shutdown_exact_verification_ns":%d}\n' \
    "$((shutdown_end_ns - start_ns))" "$((receiver_line_barrier_ns - work_start_ns))" \
    "$((generator_end_ns - work_start_ns))" "$((receiver_line_barrier_ns - generator_end_ns))" \
    "$((receiver_line_barrier_ns - work_start_ns))" "$((verify_end_ns - verify_start_ns))" > "$BENCH_METRIC_FILE"
exit_test
