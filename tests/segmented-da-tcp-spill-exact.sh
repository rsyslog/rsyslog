#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Verify a segmented disk-assisted LinkedList main queue under TCP
# backpressure. Four tcpflood generators send 10,000 independently numbered
# records through imtcp. minitcpsrv opens the omfwd listener but delays reads
# until the segmented DA child state file exists. That state file is the spill
# oracle: it proves the child materialized before output is allowed to drain.
# The standard sequence checker rejects loss, duplicates, and malformed
# framing. This is a functionality regression test; it has no throughput or
# elapsed-time oracle.

. ${srcdir:=.}/diag.sh init

NUMMESSAGES=10000
TCP_CONNECTIONS=4
PAYLOAD_BYTES=4096
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"
RECEIVER_FILE="$PWD/${RSYSLOG_DYNNAME}.receiver"
RECEIVER_PORT_FILE="$PWD/${RSYSLOG_DYNNAME}.receiver.port"
RECEIVER_READY_FILE="$PWD/${RSYSLOG_DYNNAME}.receiver.ready"
RECEIVER_RELEASE_FILE="$PWD/${RSYSLOG_DYNNAME}.receiver.release"
RECEIVER_KEEP_FILE="$PWD/${RSYSLOG_DYNNAME}.receiver.keep"
INPUT_PORT_FILE="$PWD/${RSYSLOG_DYNNAME}.input.port"
SENDER_MARKER="$PWD/${RSYSLOG_DYNNAME}.sender.marker"

rm -rf "$SPOOL_DIR"
rm -f "$INPUT_PORT_FILE" "$SENDER_MARKER" "$RECEIVER_FILE" "$RECEIVER_PORT_FILE" "$RECEIVER_READY_FILE" "$RECEIVER_RELEASE_FILE" "$RECEIVER_KEEP_FILE"
mkdir -p "$SPOOL_DIR" || error_exit 1 "cannot create spool directory"
: >"$RECEIVER_KEEP_FILE"
./minitcpsrv -t127.0.0.1 -p0 -P "$RECEIVER_PORT_FILE" -L "$RECEIVER_READY_FILE" \
	-Q "$RECEIVER_RELEASE_FILE" -K "$RECEIVER_KEEP_FILE" -f "$RECEIVER_FILE" &
RECEIVER_PID=$!
wait_file_exists "$RECEIVER_READY_FILE"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
global(workDirectory="'"$SPOOL_DIR"'")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'"$INPUT_PORT_FILE"'" workerthreads="4")
main_queue(queue.type="LinkedList" queue.filename="mainq"
	queue.size="400" queue.highWatermark="320" queue.lowWatermark="80" queue.fullDelayMark="400"
	queue.maxFileSize="64m" queue.maxDiskSpace="2g" queue.workerThreads="4"
	queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
	queue.diskQueueType="segmentedDisk" queue.diskQueueIdleTimeout="-1"
	queue.checkpointInterval="0" queue.syncQueueFiles="off")
template(name="tcpSpillFormat" type="string" string="%msg%\n")
if ($msg contains "msgnum:") then
	action(type="omfwd" target="127.0.0.1" port="'"$(cat "$RECEIVER_PORT_FILE")"'" protocol="tcp" template="tcpSpillFormat")
'
startup
wait_file_exists "$INPUT_PORT_FILE"

./tcpflood -p"$(cat "$INPUT_PORT_FILE")" -c"$TCP_CONNECTIONS" -Y -m"$NUMMESSAGES" -d"$PAYLOAD_BYTES" -q "$SENDER_MARKER" &
SENDER_PID=$!
deadline=$((SECONDS + 60))
while [ ! -e "$SPOOL_DIR/mainq.segq/state" ]; do
	# This bounded wait is a liveness ceiling, not a performance assertion.
	# Output remains blocked until state proves the DA child was used.
	[ "$SECONDS" -lt "$deadline" ] || error_exit 1 "segmented DA child did not materialize"
	./msleep 25
done
touch "$RECEIVER_RELEASE_FILE"
wait "$SENDER_PID"
if [ $? -ne 0 ] || ! tcpflood_marker_is_valid "$SENDER_MARKER"; then
	print_tcpflood_marker_file "$SENDER_MARKER"
	error_exit 1 "tcpflood did not complete cleanly"
fi
wait_queueempty
# Main-queue emptiness may precede completion of batches already owned by DA
# child workers. Shutdown is the downstream completion barrier and also closes
# omfwd's TCP stream, allowing the receiver to consume the final records.
shutdown_when_empty
wait_shutdown
wait_file_lines --abort-on-oversize "$RECEIVER_FILE" "$NUMMESSAGES" 120
rm -f "$RECEIVER_KEEP_FILE"
wait "$RECEIVER_PID" || error_exit 1 "minitcpsrv did not exit cleanly"
cut -d: -f2 "$RECEIVER_FILE" >"$RSYSLOG_OUT_LOG"
seq_check 0 $((NUMMESSAGES - 1))
rm -rf "$SPOOL_DIR"
exit_test
