#!/bin/bash
# Shared deterministic crash-point driver. Phase one acknowledges a known
# multi-segment backlog while the first worker is delayed, arms one imdiag
# point, and requires SIGKILL-style termination. Restart must recover the full
# acknowledged sequence; duplicates are allowed by checkpoint replay. The
# post-unlink oracle polls durable state because output becomes visible before
# the completing worker finishes the pending-delete state transaction.
. ${srcdir:=.}/diag.sh init
check_command_available python3
require_plugin omtesting
: "${SEGDISK_FAULT_POINT:?SEGDISK_FAULT_POINT is required}"
: "${SEGDISK_FAULT_REPEATS:=1}"
export NUMMESSAGES=400
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"

write_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'$SPOOL_DIR'")
main_queue(queue.type="segmentedDisk" queue.filename="mainq"
	queue.maxFileSize="1k" queue.dequeueBatchSize="1"
	queue.workerThreads="1" queue.checkpointInterval="1"
	queue.saveOnShutdown="on")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
'"$1"'
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
}

write_conf ':omtesting:sleep 10 0'
startup
injectmsg 0 "$NUMMESSAGES"

crash_once() {
	set_segmented_disk_fault "$SEGDISK_FAULT_POINT"
	export RSTB_EXPECT_NO_PROPER_TERMINATION=YES
	crashed_pid=$(getpid)
	case "$SEGDISK_FAULT_POINT" in
	reservation-published|segment-created|seal-written|seal-renamed)
		printf -v trigger 'trigger-%0900d' 0
		printf 'injectmsg literal %s\ninjectmsg literal %s\n' "$trigger" "$trigger" |
			$TESTTOOL_DIR/diagtalker -p"$IMDIAG_PORT" >/dev/null 2>&1 || true
		;;
	esac
	# A pre-delete fault is reached only after the oldest segment drains. Allow
	# two minutes so sanitizer builds running several crash tests concurrently
	# can reach the hook; successful runs exit this loop as soon as SIGKILL is
	# observed, so the larger ceiling does not delay the normal oracle.
	for _ in {1..1200}; do
		if ! kill -0 "$crashed_pid" 2>/dev/null; then
			break
		fi
		$TESTTOOL_DIR/msleep 100
	done
	if kill -0 "$crashed_pid" 2>/dev/null; then
		echo "FAIL: segmentedDisk fault point $SEGDISK_FAULT_POINT did not terminate rsyslogd"
		error_exit 1
	fi
	wait "$crashed_pid" 2>/dev/null || true
	rm -f "$RSYSLOG_PIDBASE.pid"
}

crash_once
for ((repeat = 2; repeat <= SEGDISK_FAULT_REPEATS; ++repeat)); do
	write_conf ':omtesting:sleep 10 0'
	startup
	crash_once
done

write_conf '# no delay after crash'
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check_dupes
wait_seq_check_dupes() {
	wait_seq_check 0 $((NUMMESSAGES - 1)) -d
}

wait_pending_delete_cleared() {
	local state_file="${RSYSLOG_DYNNAME}.post-unlink.json"
	for _ in {1..300}; do
		if python3 "$srcdir/segdisk-inspect.py" "$SPOOL_DIR/mainq.segq" >"$state_file" 2>/dev/null && python3 -c '
import json, sys
d = json.load(open(sys.argv[1], encoding="utf-8"))
slots = [s for s in d["state_slots"] if s["valid"]]
newest = max(slots, key=lambda s: s["generation"])
assert newest["delete_first"] == 0 and newest["delete_last"] == 0
assert not [s for s in d["segments"] if s["id"] < newest["first_live_segment"]]
' "$state_file" 2>/dev/null; then
			return 0
		fi
		$TESTTOOL_DIR/msleep 100
	done
	echo "FAIL: segmentedDisk pending deletion did not clear after recovery"
	cat "$state_file"
	return 1
}

startup
if [ "$SEGDISK_FAULT_POINT" = segment-unlinked ]; then
	# The crash occurs after unlink but before pending-delete state can be
	# cleared. Output can become visible before the worker completes its batch,
	# so poll the exact durable state/file invariant rather than sampling the
	# legitimate in-progress transaction. The 30-second retry window bounds a real
	# cleanup failure while tolerating loaded CI runners. This runs before clean
	# shutdown so spool removal cannot hide a missed cleanup retry.
	wait_seq_check 0 $((NUMMESSAGES - 1)) -d
	wait_pending_delete_cleared || error_exit 1
fi
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1)) -d
rm -rf "$SPOOL_DIR"
exit_test
