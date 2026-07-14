#!/bin/bash
# Keep a segmented DA action in retry state for longer than its 100 ms idle
# timeout. The store must remain materialized while retry-before-commit owns
# records; after the output directory appears, exact drain plus store removal
# proves cleanup begins only after the retry batch is durably completed.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"
SEG_DIR="$SPOOL_DIR/retryq.segq"
OUTPUT_DIR="$PWD/${RSYSLOG_DYNNAME}.missing"
OUTPUT_FILE="$OUTPUT_DIR/output.log"

wait_path_state() {
	local mode="$1" path="$2" deadline=$((SECONDS + 30))
	while true; do
		[ "$mode" = present ] && [ -e "$path" ] && return
		[ "$mode" = absent ] && [ ! -e "$path" ] && return
		[ "$SECONDS" -lt "$deadline" ] || error_exit 1 "path did not become $mode: $path"
		./msleep 25
	done
}

generate_conf
add_conf '
global(workDirectory="'"$SPOOL_DIR"'")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'"$OUTPUT_FILE"'" template="outfmt"
	createDirs="off" action.resumeRetryCount="-1" action.resumeInterval="1"
	queue.type="LinkedList" queue.filename="retryq"
	queue.size="50" queue.highWatermark="10" queue.lowWatermark="5"
	queue.dequeueBatchSize="1" queue.diskQueueType="segmentedDisk"
	queue.diskQueueIdleTimeout="100")
'

startup
injectmsg
wait_path_state present "$SEG_DIR"
./msleep 500
[ -d "$SEG_DIR" ] || error_exit 1 "idle cleanup removed a store with retry records"
mkdir "$OUTPUT_DIR"
wait_file_lines "$OUTPUT_FILE" "$NUMMESSAGES" 300
wait_path_state absent "$SEG_DIR"
shutdown_when_empty
wait_shutdown
RSYSLOG_OUT_LOG="$OUTPUT_FILE" seq_check
exit_test
