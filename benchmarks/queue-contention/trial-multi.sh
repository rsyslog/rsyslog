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
: "${BENCH_SCOPE:=global}" "${BENCH_FRONTEND_SIZE:=}" "${BENCH_MAX_FRONTENDS:=}"
: "${BENCH_CONFIG_ONLY:=no}" "${BENCH_OUTPUT:=omfile}"
case "$BENCH_OUTPUT" in
omfile|omfwd) ;;
*) echo "BENCH_OUTPUT must be omfile or omfwd" >&2; exit 1 ;;
esac
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
case "$BENCH_SCOPE" in
global)
    # Keep this emitted FixedArray configuration byte-for-byte identical to
    # the frozen S0 control. In particular, S0 does not know local options.
    BENCH_LOCAL_QUEUE_CONF=''
    BENCH_TOTAL_SLOT_BOUND=$BENCH_QUEUE_SIZE
    ;;
local)
    if [[ ! "$BENCH_FRONTEND_SIZE" =~ ^[1-9][0-9]*$ || ! "$BENCH_MAX_FRONTENDS" =~ ^[1-9][0-9]*$ ]]; then
        echo "local scope requires positive BENCH_FRONTEND_SIZE and BENCH_MAX_FRONTENDS" >&2
        exit 1
    fi
    BENCH_LOCAL_QUEUE_CONF=' queue.scope="local" queue.local.frontendSize="'$BENCH_FRONTEND_SIZE'" queue.local.maxFrontends="'$BENCH_MAX_FRONTENDS'"'
    BENCH_TOTAL_SLOT_BOUND=$((BENCH_QUEUE_SIZE + BENCH_MAX_FRONTENDS * (BENCH_FRONTEND_SIZE + BENCH_DEQUEUE_BATCH_SIZE)))
    ;;
*)
    echo "BENCH_SCOPE must be global or local" >&2
    exit 1
    ;;
esac
# The fragment is inserted into RainerScript, so shellcheck cannot parse its
# deliberate embedded quoting. The global string matches the frozen S0 stanza.
# shellcheck disable=SC2089
MAIN_QUEUE_CONF='main_queue(queue.type="FixedArray" queue.size="'$BENCH_QUEUE_SIZE'"
    queue.workerThreads="'$BENCH_CONSUMER_WORKERS'" queue.workerThreadMinimumMessages="'$BENCH_WORKER_MINIMUM'"
    queue.dequeueBatchSize="'$BENCH_DEQUEUE_BATCH_SIZE'" queue.mutexContentionStats="'$BENCH_MUTEX_CONTENTION_STATS'"'$BENCH_LOCAL_QUEUE_CONF')'
case "$BENCH_CONFIG_ONLY" in
yes)
    printf 'scope=%s\nbackend_queue_size=%s\ndequeue_batch_size=%s\ntotal_slot_bound=%s\nmain_queue_conf=%s\n' \
        "$BENCH_SCOPE" "$BENCH_QUEUE_SIZE" "$BENCH_DEQUEUE_BATCH_SIZE" "$BENCH_TOTAL_SLOT_BOUND" "$MAIN_QUEUE_CONF"
    if [[ "$BENCH_SCOPE" == local ]]; then
        printf 'frontend_size=%s\nmax_frontends=%s\n' "$BENCH_FRONTEND_SIZE" "$BENCH_MAX_FRONTENDS"
    fi
    exit 0
    ;;
no) ;;
*)
    echo "BENCH_CONFIG_ONLY must be yes or no" >&2
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
case "$BENCH_SCOPE" in
global)
    BENCH_OMFILE_PATH=$RSYSLOG_OUT_LOG
    ;;
local)
    # Local omfile activation preopens every omfile action. Qualify both the
    # diagnostic preamble and benchmark sink before input startup; the global
    # control keeps its frozen generated configuration unchanged.
    BENCH_OMFILE_PATH="$PWD/$RSYSLOG_OUT_LOG"
    sed -i '1i template(name="localdiag" type="string" string="%msg%\\n")' "${TESTCONF_NM}.conf"
    sed -i "s|file=\"./$RSYSLOG_DYNNAME.started\"|file=\"$PWD/$RSYSLOG_DYNNAME.started\" template=\"localdiag\"|" \
        "${TESTCONF_NM}.conf"
    ;;
esac
if [[ "$BENCH_IMPSTATS" == yes ]]; then
    add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATS_FILE'" interval="1" format="json" log.syslog="off")
'
fi
# The default omfile stanza remains unchanged. TCP uses a supervised receiver
# without artificial delays; -K retains the listener through daemon shutdown.
# The readiness marker acknowledges listen(), not just socket allocation.
BENCH_OUTPUT_ACTION='action(type="omfile" file="'$BENCH_OMFILE_PATH'" template="outfmt")'
if [[ "$BENCH_OUTPUT" == omfwd ]]; then
    RECEIVER_KEEP="$PWD/$RSYSLOG_DYNNAME.receiver.keep"
    RECEIVER_READY="$PWD/$RSYSLOG_DYNNAME.receiver.ready"
    RECEIVER_PORT="$PWD/$RSYSLOG_DYNNAME.receiver.port"
    RECEIVER_LOG="$PWD/$RSYSLOG_DYNNAME.receiver.log"
    touch "$RECEIVER_KEEP"
    ./minitcpsrv -t127.0.0.1 -p0 -P "$RECEIVER_PORT" -L "$RECEIVER_READY" \
        -K "$RECEIVER_KEEP" -f "$RSYSLOG_OUT_LOG" >"$RECEIVER_LOG" 2>&1 &
    RECEIVER_PID=$!
    MINITCPSRVR_PIDS="${MINITCPSRVR_PIDS:-} $RECEIVER_PID"
    wait_file_exists "$RECEIVER_READY"
    assign_file_content OUTPUT_PORT "$RECEIVER_PORT"
    BENCH_OUTPUT_ACTION='action(type="omfwd" target="127.0.0.1" port="'$OUTPUT_PORT'" protocol="tcp" streamDriver="ptcp" streamDriver.mode="0" template="outfmt")'
fi
BENCH_RAINERSCRIPT='
global(processInternalMessages="off" abortOnUncleanConfig="on")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
    listenPortFileName="'$PORT_FILE'" workerThreads="'$BENCH_INPUT_WORKERS'")
'
BENCH_RAINERSCRIPT+="$MAIN_QUEUE_CONF"
BENCH_RAINERSCRIPT+='
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then {
    set $.parseStatus = parse_json("{\"nested\":{\"array\":[1,2,3,4,5,6,7,8],\"text\":\"queue contention benchmark payload\"}}", "\$!payload");
    if ($.parseStatus == 0 and $!payload!nested!text == "queue contention benchmark payload") then
        '"$BENCH_OUTPUT_ACTION"'
}
'
add_conf "$BENCH_RAINERSCRIPT"
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
if [[ "$BENCH_OUTPUT" == omfwd ]]; then
    rm -f "$RECEIVER_KEEP"
    wait "$RECEIVER_PID" || error_exit 1 "TCP receiver did not exit cleanly"
    MINITCPSRVR_PIDS=""
    if grep -q 'connection limit reached' "$RECEIVER_LOG"; then
        error_exit 1 "TCP receiver dropped an excess connection"
    fi
fi
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
