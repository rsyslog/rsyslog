#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Latency observation is separate from the throughput trial. A single Python
# process timestamps each exact ID immediately before sendall and when its
# complete omfile line is observed. The Python oracle rejects loss, duplicates,
# invalid output, sender lateness, offered-rate drift, and polling-gap excess.
export NUMMESSAGES=${BENCH_MESSAGES:-100000}
OBSERVER_DIR=${BENCH_CAMPAIGN_DIR:-$(cd "$(dirname "$0")" && pwd)}
: "${BENCH_INPUT_WORKERS:=8}" "${BENCH_CONSUMER_WORKERS:=4}" "${BENCH_CONNECTIONS:=16}"
: "${BENCH_PAYLOAD:=512}" "${BENCH_OFFERED_RATE:=10000}" "${BENCH_QUEUE_SIZE:=32768}"
: "${BENCH_DEQUEUE_BATCH_SIZE:=1024}" "${BENCH_WORKER_MINIMUM:=1024}" "${BENCH_SCOPE:=global}"
: "${BENCH_FRONTEND_CAPACITY:=10000}" "${BENCH_FRONTEND_MAX:=8}"
: "${BENCH_OMFILE_FLUSH_POLICY:=sync}" "${BENCH_POLL_US:=100}"
: "${BENCH_MAX_LATENESS_US:=400}" "${BENCH_MAX_POLL_GAP_US:=400}"
# This is deliberately an rsyslog configuration fragment, not shell code.
# shellcheck disable=SC2089
case "$BENCH_SCOPE" in
    global) QUEUE_SCOPE_CONF='queue.scope="global"' ;;
    local) QUEUE_SCOPE_CONF="queue.scope=\"local\" queue.local.frontendSize=\"$BENCH_FRONTEND_CAPACITY\" queue.local.maxFrontends=\"$BENCH_FRONTEND_MAX\"" ;;
    *) echo "BENCH_SCOPE must be global or local" >&2; exit 1 ;;
esac
if [[ "$BENCH_OMFILE_FLUSH_POLICY" != sync ]]; then
    echo "only matched synchronous omfile flushing is currently supported" >&2
    exit 1
fi
. ${srcdir:=.}/diag.sh init
PORT_FILE="$PWD/$RSYSLOG_DYNNAME.latency.port"
generate_conf
# The quoted fragment below is intentionally emitted into RainerScript.
# shellcheck disable=SC2090
add_conf '
global(processInternalMessages="off")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$PORT_FILE'" workerThreads="'$BENCH_INPUT_WORKERS'")
main_queue(queue.type="FixedArray" queue.size="'$BENCH_QUEUE_SIZE'" queue.workerThreads="'$BENCH_CONSUMER_WORKERS'" queue.workerThreadMinimumMessages="'$BENCH_WORKER_MINIMUM'" queue.dequeueBatchSize="'$BENCH_DEQUEUE_BATCH_SIZE'" '$QUEUE_SCOPE_CONF')
template(name="latencyfmt" type="string" string="%msg%\n")
if ($msg startswith "latency:") then { action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="latencyfmt") }
'
startup
assign_file_content INPUT_PORT "$PORT_FILE"
rm -f "$BENCH_METRIC_FILE"
python3 "$OBSERVER_DIR/latency-observer.py" --host 127.0.0.1 --port "$INPUT_PORT" --output "$RSYSLOG_OUT_LOG" \
    --result "$BENCH_METRIC_FILE" --messages "$NUMMESSAGES" --connections "$BENCH_CONNECTIONS" --rate "$BENCH_OFFERED_RATE" \
    --payload "$BENCH_PAYLOAD" --poll-us "$BENCH_POLL_US" --max-lateness-us "$BENCH_MAX_LATENESS_US" \
    --max-poll-gap-us "$BENCH_MAX_POLL_GAP_US" --allow-invalid || exit 1
shutdown_when_empty
wait_shutdown
python3 "$OBSERVER_DIR/latency-observer.py" --finalize --output "$RSYSLOG_OUT_LOG" --result "$BENCH_METRIC_FILE" \
    --messages "$NUMMESSAGES" --payload "$BENCH_PAYLOAD" || exit 1
exit_test
