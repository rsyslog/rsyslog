#!/bin/bash
# Corrupt more than the 1 MiB per-attempt recovery budget in a sealed segment.
# Recovery must yield and retain its cursor, accept new producer data, keep the
# queue non-empty, and eventually deliver records from later segments. Counter
# values and delivery of the post-restart message are the deterministic oracle.
. ${srcdir:=.}/diag.sh init
check_command_available python3
require_plugin impstats
export NUMMESSAGES=10000
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"
STATS_FILE="$PWD/${RSYSLOG_DYNNAME}.stats.log"

write_conf() {
	local queue_config='main_queue(queue.type="segmentedDisk" queue.filename="mainq"
	queue.maxFileSize="1200k" queue.dequeueBatchSize="32" queue.saveOnShutdown="on")'
	if [ "${SEGDISK_RECOVERY_DA:-0}" = 1 ] && [ "$2" = recovery ]; then
		queue_config='main_queue(queue.type="LinkedList" queue.filename="mainq"
		queue.size="50" queue.highWatermark="10" queue.lowWatermark="5"
		queue.maxFileSize="1200k" queue.dequeueBatchSize="32" queue.saveOnShutdown="on"
		queue.diskQueueType="auto" queue.diskQueueIdleTimeout="-1")'
	fi
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATS_FILE'" interval="1")
global(workDirectory="'$SPOOL_DIR'")
'"$queue_config"'
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
'"$1"'
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
}

write_conf ':omtesting:sleep 10 0' seed
startup
wait_rsyslog_instance_pid
injectmsg
. "$srcdir/diag.sh" kill-immediate
wait_shutdown
python3 "$srcdir/segdisk-inspect.py" "$SPOOL_DIR/mainq.segq" >"${RSYSLOG_DYNNAME}.budget-before.json"
target_id=$(python3 -c 'import json,os,sys; d=json.load(open(sys.argv[1])); c=[s for s in d["segments"] if s["sealed"] and os.path.getsize(s["path"])>1048576]; print(c[0]["id"])' \
	"${RSYSLOG_DYNNAME}.budget-before.json")
target_records=$(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); target=int(sys.argv[2]); print(len(next(s for s in d["segments"] if s["id"]==target)["records"]))' \
	"${RSYSLOG_DYNNAME}.budget-before.json" "$target_id")
python3 "$srcdir/segdisk-inspect.py" "$SPOOL_DIR/mainq.segq" --corrupt-segment-framing "$target_id" \
	>"${RSYSLOG_DYNNAME}.budget-corrupt.json"

: >"$STATS_FILE"
write_conf '# no delay during bounded recovery' recovery
startup
injectmsg 10000 1
export NUMMESSAGES=$((10001 - target_records))
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
shutdown_when_empty
wait_shutdown
content_check '00010000' "$RSYSLOG_OUT_LOG"
pending=$(grep 'recovery.pending=' "$STATS_FILE" | tail -n 1 |
	sed -E 's/.*recovery\.pending=([0-9]+).*/\1/')
bytes=$(grep 'recovery.bytes=' "$STATS_FILE" | tail -n 1 |
	sed -E 's/.*recovery\.bytes=([0-9]+).*/\1/')
if [ "${pending:-0}" -lt 1 ] || [ "${bytes:-0}" -le 1048576 ]; then
	printf 'FAIL: bounded recovery counters pending=%s bytes=%s\n' "${pending:-missing}" "${bytes:-missing}"
	cat "$STATS_FILE"
	error_exit 1
fi
rm -rf "$SPOOL_DIR"
exit_test
