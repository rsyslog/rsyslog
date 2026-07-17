#!/bin/bash
# Execute one full-lifecycle benchmark trial with exact-delivery validation.
: "${BENCH_BUILD_DIR:?}"
: "${BENCH_SCENARIO:?}"
: "${BENCH_MODE:?}"
: "${BENCH_PAYLOAD_BYTES:?}"
: "${BENCH_MESSAGES:?}"
: "${BENCH_SYNC_FILES:?}"
: "${BENCH_BATCH_SIZE:?}"
: "${BENCH_SEGMENT_BYTES:?}"
: "${BENCH_METRIC_FILE:?}"

cd "$BENCH_BUILD_DIR/tests" || exit 1
export srcdir="$BENCH_BUILD_DIR/tests"
. "$srcdir/diag.sh" init

SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"
RECEIVER_FILE="$PWD/${RSYSLOG_DYNNAME}.receiver"
RECEIVER_PORT_FILE="$PWD/${RSYSLOG_DYNNAME}.receiver.port"
RECEIVER_READY_FILE="$PWD/${RSYSLOG_DYNNAME}.receiver.ready"
RECEIVER_LISTEN_RELEASE_FILE="$PWD/${RSYSLOG_DYNNAME}.receiver.listen.release"
RECEIVER_RELEASE_FILE="$PWD/${RSYSLOG_DYNNAME}.receiver.release"
RECEIVER_KEEP_FILE="$PWD/${RSYSLOG_DYNNAME}.receiver.keep"
INPUT_PORT_FILE="$PWD/${RSYSLOG_DYNNAME}.input.port"
SENDER_MARKER="$PWD/${RSYSLOG_DYNNAME}.sender.marker"
STATS_FILE="$PWD/${RSYSLOG_DYNNAME}.stats"

cleanup_benchmark() {
	rm -f "$RECEIVER_KEEP_FILE"
	if [ -n "${RECEIVER_PID:-}" ] && kill -0 "$RECEIVER_PID" 2>/dev/null; then
		kill "$RECEIVER_PID" 2>/dev/null || :
	fi
	rm -rf "$SPOOL_DIR"
	rm -f "$STATS_FILE"
}
trap cleanup_benchmark EXIT

rm -rf "$SPOOL_DIR"
mkdir -p "$SPOOL_DIR"
: >"$RECEIVER_KEEP_FILE"
./minitcpsrv -t127.0.0.1 -p0 -b4096 -P "$RECEIVER_PORT_FILE" -L "$RECEIVER_READY_FILE" \
	-w "$RECEIVER_LISTEN_RELEASE_FILE" \
	-Q "$RECEIVER_RELEASE_FILE" -K "$RECEIVER_KEEP_FILE" -f "$RECEIVER_FILE" &
RECEIVER_PID=$!
wait_file_exists "$RECEIVER_PORT_FILE"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" log.file="'"$STATS_FILE"'"
	interval="1" resetCounters="off" ruleset="benchStats")
ruleset(name="benchStats") { stop }
global(workDirectory="'"$SPOOL_DIR"'")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'"$INPUT_PORT_FILE"'" workerthreads="4")
'
worker_minimum=1024
if [ "$BENCH_MODE" = segmented ]; then
	add_conf 'main_queue(queue.type="segmentedDisk" queue.filename="mainq"
	queue.maxFileSize="'"$BENCH_SEGMENT_BYTES"'" queue.maxDiskSpace="2g" queue.dequeueBatchSize="'"$BENCH_BATCH_SIZE"'"
	queue.workerThreadMinimumMessages="'"$worker_minimum"'"
	queue.checkpointInterval="0" queue.syncQueueFiles="'"$BENCH_SYNC_FILES"'" queue.saveOnShutdown="on")'
else
	add_conf 'main_queue(queue.type="LinkedList" queue.filename="mainq"
	queue.size="8192" queue.highWatermark="1024" queue.lowWatermark="256" queue.fullDelayMark="8192"
	queue.maxFileSize="'"$BENCH_SEGMENT_BYTES"'" queue.maxDiskSpace="2g" queue.workerThreads="4"
	queue.workerThreadMinimumMessages="'"$worker_minimum"'" queue.dequeueBatchSize="'"$BENCH_BATCH_SIZE"'" queue.timeoutEnqueue="300000"
	queue.diskQueueType="segmentedDisk" queue.diskQueueIdleTimeout="-1"
	queue.checkpointInterval="0" queue.syncQueueFiles="'"$BENCH_SYNC_FILES"'" queue.saveOnShutdown="on")'
fi
add_conf '
	template(name="benchFormat" type="string" string="%msg%\n")
'
add_conf '
if ($msg contains "msgnum:") then
	action(type="omfwd" target="127.0.0.1" port="'"$(cat "$RECEIVER_PORT_FILE")"'" protocol="tcp"
		template="benchFormat" action.resumeInterval="1" action.resumeRetryCount="-1")
'

if [ -n "${BENCH_STRACE_FILE:-}" ]; then
	# diag.sh startup() consumes this command prefix.
	# shellcheck disable=SC2034
	valgrind="strace -f -c -o '$BENCH_STRACE_FILE'"
fi

now_ns() { date +%s%N; }
t0=$(now_ns)
startup
wait_file_exists "$INPUT_PORT_FILE"
t_started=$(now_ns)

./tcpflood -p"$(cat "$INPUT_PORT_FILE")" -c4 -Y -m"$BENCH_MESSAGES" -d"$BENCH_PAYLOAD_BYTES" \
	-q "$SENDER_MARKER" >/dev/null &
SENDER_PID=$!
deadline=$((SECONDS + 300))
while [ ! -e "$SPOOL_DIR/mainq.segq/state" ]; do
	[ "$SECONDS" -lt "$deadline" ] || error_exit 1 "segmented queue did not materialize"
	./msleep 10
done
wait "$SENDER_PID"
tcpflood_marker_is_valid "$SENDER_MARKER" || error_exit 1 "tcpflood did not complete cleanly"

# Sender completion only proves that the kernel accepted the TCP stream. The
# imtcp submitted counter proves that every input message reached rsyslog and is
# emitted through a direct impstats path, outside the queue under test.
deadline=$((SECONDS + 300))
input_submitted=0
while [ "$input_submitted" -lt "$BENCH_MESSAGES" ]; do
	if [ -f "$STATS_FILE" ]; then
		input_submitted=$(sed -n 's/.*imtcp(0): origin=imtcp submitted=\([0-9][0-9]*\).*/\1/p' \
			"$STATS_FILE" | tail -1)
		[ -n "$input_submitted" ] || input_submitted=0
	fi
	[ "$SECONDS" -lt "$deadline" ] || error_exit 1 "imtcp did not report all messages submitted"
	./msleep 20
done
enqueued=$(sed -n 's/.*main Q: origin=core.queue .* enqueued=\([0-9][0-9]*\).*/\1/p' "$STATS_FILE" | tail -1)
[ -n "$enqueued" ] || error_exit 1 "main queue did not report its admission count"

# DA admission precedes asynchronous transfer into the segmented child. Wait
# until the child's cumulative admission count stops growing, and require a
# nonzero count so the scenario cannot silently cease to exercise disk assist.
backlog=$(get_mainqueuesize)
backlog=${backlog##*: }
child_enqueued=0
if [ "$BENCH_MODE" = da ]; then
	stable_samples=0
	previous_child_enqueued=-1
	deadline=$((SECONDS + 300))
	while [ "$stable_samples" -lt 100 ]; do
		if [ -f "$STATS_FILE" ]; then
			child_enqueued=$(sed -n 's/.*main Q\[DA\]: origin=core.queue .* enqueued=\([0-9][0-9]*\).*/\1/p' \
				"$STATS_FILE" | tail -1)
			[ -n "$child_enqueued" ] || child_enqueued=0
		fi
		if [ "$child_enqueued" -gt 0 ] && [ "$child_enqueued" -eq "$previous_child_enqueued" ]; then
			stable_samples=$((stable_samples + 1))
		else
			stable_samples=0
		fi
		previous_child_enqueued=$child_enqueued
		[ "$SECONDS" -lt "$deadline" ] || error_exit 1 "DA child spill count did not reach a stable nonzero value"
		./msleep 20
	done
	backlog=$(get_mainqueuesize)
	backlog=${backlog##*: }
fi
t_spilled=$(now_ns)
segments_at_spill=$(find "$SPOOL_DIR/mainq.segq" -maxdepth 1 -type f \
	\( -name '*.seg' -o -name '*.open' -o -name '*.recover' \) 2>/dev/null | wc -l)

restart_ns=0
t_drain_start=$(now_ns)
touch "$RECEIVER_LISTEN_RELEASE_FILE" "$RECEIVER_RELEASE_FILE"
wait_file_exists "$RECEIVER_READY_FILE"
wait_file_lines --abort-on-oversize "$RECEIVER_FILE" "$BENCH_MESSAGES" 300
t_drained=$(now_ns)
shutdown_when_empty
wait_shutdown
rm -f "$RECEIVER_KEEP_FILE"
wait "$RECEIVER_PID" || error_exit 1 "minitcpsrv did not exit cleanly"
t_end=$(now_ns)

cut -d: -f2 "$RECEIVER_FILE" >"$RSYSLOG_OUT_LOG"
export NUMMESSAGES="$BENCH_MESSAGES"
seq_check 0 $((BENCH_MESSAGES - 1))
mkdir -p "$(dirname "$BENCH_METRIC_FILE")"
printf '{"scenario":"%s","mode":"%s","payload_bytes":%d,"batch_size":%d,"segment_bytes":%d,"messages":%d,"bytes":%d,"input_submitted_at_spill":%d,"enqueued_at_spill":%d,"child_enqueued_at_spill":%d,"backlog_at_spill":%d,"startup_ns":%d,"spill_ns":%d,"restart_ns":%d,"drain_ns":%d,"shutdown_ns":%d,"end_to_end_ns":%d,"segments_observed":%d}\n' \
	"$BENCH_SCENARIO" "$BENCH_MODE" "$BENCH_PAYLOAD_BYTES" "$BENCH_BATCH_SIZE" "$BENCH_SEGMENT_BYTES" \
	"$BENCH_MESSAGES" "$((BENCH_PAYLOAD_BYTES * BENCH_MESSAGES))" "$input_submitted" "$enqueued" \
	"$child_enqueued" "$backlog" \
	"$((t_started - t0))" \
	"$((t_spilled - t_started))" \
	"$restart_ns" "$((t_drained - t_drain_start))" "$((t_end - t_drained))" "$((t_end - t0))" \
	"$segments_at_spill" \
	>"$BENCH_METRIC_FILE"
exit_test
