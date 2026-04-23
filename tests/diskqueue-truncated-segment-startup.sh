#!/bin/bash
# Validate queue.onCorruption handling when bytes inside a persisted disk-queue
# segment are corrupted in the middle of the queue sequence. Restart should either
# quarantine the corrupted queue into mainq.bad.* or finish draining it
# without leaving live queue files behind.
# added 2026-04-20 by Codex, released under ASL 2.0

export TEST_MAX_RUNTIME=${TEST_MAX_RUNTIME:-300}

. ${srcdir:=.}/diag.sh init

BASE_NUMMESSAGES=${NUMMESSAGES:-8000}
PHASE1_SLEEP_USEC=${PHASE1_SLEEP_USEC:-5000}
PREPARE_ATTEMPTS=${PREPARE_ATTEMPTS:-3}
RECOVERY_WAIT_TIMEOUT=${RECOVERY_WAIT_TIMEOUT:-120}
SHUTDOWN_TIMEOUT=${SHUTDOWN_TIMEOUT:-60}
NUMMESSAGES=$BASE_NUMMESSAGES
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"
RSYSLOGD_LOG="${RSYSLOG_DYNNAME}.rsyslogd.log"
STARTED_LOG="${RSYSLOG_DYNNAME}.started"

write_phase1_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'"$SPOOL_DIR"'")

main_queue(
	queue.type="disk"
	queue.filename="mainq"
	queue.maxFileSize="16k"
	queue.saveOnShutdown="on"
	queue.timeoutShutdown="1"
)

template(name="outfmt" type="string" string="%msg:F,58:2%\\n")
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`)

:omtesting:sleep 0 '"$PHASE1_SLEEP_USEC"'
if ($msg contains "msgnum:") then {
	action(type="omfile" template="outfmt" dynaFile="dynfile")
}
'
}

write_phase2_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'"$SPOOL_DIR"'")

main_queue(
	queue.type="disk"
	queue.filename="mainq"
	queue.maxFileSize="16k"
	queue.saveOnShutdown="on"
	queue.onCorruption="safe"
)

template(name="outfmt" type="string" string="%msg:F,58:2%\\n")
action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'
}

list_spool_top() {
	for path in "$SPOOL_DIR"/*; do
		[ -e "$path" ] || continue
		base="${path##*/}"
		if [ -f "$path" ]; then
			size=$(wc -c < "$path")
			printf '%s %s bytes\n' "$base" "$size"
		elif [ -L "$path" ]; then
			printf '%s symlink\n' "$base"
		else
			printf '%s/\n' "$base"
		fi
	done | sort
}

count_bad_dirs() {
	count=0
	for path in "$SPOOL_DIR"/mainq.bad.*; do
		[ -d "$path" ] || continue
		count=$((count + 1))
	done
	printf '%s\n' "$count"
}

count_live_mainq_files() {
	count=0
	for path in "$SPOOL_DIR"/mainq*; do
		[ -e "$path" ] || continue
		[ -f "$path" ] || [ -L "$path" ] || continue
		count=$((count + 1))
	done
	printf '%s\n' "$count"
}

count_segment_files() {
	count=0
	for path in "$SPOOL_DIR"/mainq.*; do
		[ -f "$path" ] || continue
		base="${path##*/}"
		num="${base#mainq.}"
		case "$num" in
		''|*[!0-9]*)
			continue
			;;
		esac
		count=$((count + 1))
	done
	printf '%s\n' "$count"
}

check_no_live_mainq_files() {
	live_list="${RSYSLOG_DYNNAME}.live-mainq"
	: > "$live_list"
	for path in "$SPOOL_DIR"/mainq*; do
		[ -e "$path" ] || continue
		[ -f "$path" ] || [ -L "$path" ] || continue
		printf '%s\n' "${path##*/}" >> "$live_list"
	done
	if [ -s "$live_list" ]; then
		echo "FAIL: live mainq files remain after restart recovery"
		cat "$live_list"
		ls -l "$SPOOL_DIR"
		rm -f "$live_list"
		error_exit 1
	fi
	rm -f "$live_list"
}

corrupt_middle_segment() {
	seg_list="${RSYSLOG_DYNNAME}.segments"
	sorted_list="${seg_list}.sorted"
	: > "$seg_list"
	for path in "$SPOOL_DIR"/mainq.*; do
		[ -f "$path" ] || continue
		base="${path##*/}"
		num="${base#mainq.}"
		case "$num" in
		''|*[!0-9]*)
			continue
			;;
		esac
		printf '%s\n' "$base" >> "$seg_list"
	done

	sort -t. -k2,2n "$seg_list" > "$sorted_list"
	seg_count=$(wc -l < "$sorted_list")
	if [ "$seg_count" -lt 3 ]; then
		printf 'FAIL: expected at least 3 queue segment files, found %s\n' "$seg_count"
		list_spool_top
		rm -f "$seg_list" "$sorted_list"
		error_exit 1
	fi

	mid_line=$(( (seg_count / 2) + 1 ))
	target_base=$(sed -n "${mid_line}p" "$sorted_list")
	target="$SPOOL_DIR/$target_base"
	orig_size=$(wc -c < "$target")
	seek_offs=$(( orig_size / 2 ))
	corrupt_len=4096
	if [ "$seek_offs" -le 0 ] || [ "$seek_offs" -ge "$orig_size" ]; then
		printf 'FAIL: invalid corruption offset for %s (size=%s)\n' "$target" "$orig_size"
		rm -f "$seg_list" "$sorted_list"
		error_exit 1
	fi
	if [ $(( seek_offs + corrupt_len )) -gt "$orig_size" ]; then
		corrupt_len=$(( orig_size - seek_offs ))
	fi

	printf 'Corrupting queue segment %s at offset %s for %s bytes\n' "$target" "$seek_offs" "$corrupt_len"
	dd if=/dev/zero of="$target" bs=1 seek="$seek_offs" count="$corrupt_len" conv=notrunc \
		>/dev/null 2>&1
	rm -f "$seg_list" "$sorted_list"
}

prepare_corrupted_queue() {
	attempt=1
	current_messages=$BASE_NUMMESSAGES

	while [ "$attempt" -le "$PREPARE_ATTEMPTS" ]; do
		rm -rf "$SPOOL_DIR" "$RSYSLOG_OUT_LOG" "$RSYSLOGD_LOG" "$STARTED_LOG"
		NUMMESSAGES=$current_messages

		write_phase1_conf
		startup
		injectmsg 0 "$NUMMESSAGES"
		shutdown_immediate
		wait_shutdown "" "$SHUTDOWN_TIMEOUT"

		seg_count=$(count_segment_files)
		if [ -f "$SPOOL_DIR/mainq.qi" ] && [ "$seg_count" -ge 3 ]; then
			printf 'Prepared persisted queue with %s messages and %s segment files\n' \
				"$NUMMESSAGES" "$seg_count"
			return 0
		fi

		printf 'Phase 1 attempt %s produced %s segment files with %s messages; retrying\n' \
			"$attempt" "$seg_count" "$NUMMESSAGES"
		attempt=$((attempt + 1))
		current_messages=$((current_messages + BASE_NUMMESSAGES / 2))
	done

	printf 'FAIL: unable to create a persisted queue with at least 3 segment files after %s attempts\n' \
		"$PREPARE_ATTEMPTS"
	list_spool_top
	error_exit 1
}

wait_for_recovery_outcome() {
	bad_before=$1
	deadline=$(( $(date +%s) + RECOVERY_WAIT_TIMEOUT ))

	while [ $(date +%s) -le "$deadline" ]; do
		bad_after=$(count_bad_dirs)
		if [ "$bad_after" -gt "$bad_before" ]; then
			echo "Detected mainq.bad.* quarantine directory"
			return 0
		fi

		seg_after=$(count_segment_files)
		if [ "$seg_after" -le 1 ] && [ -s "$RSYSLOG_OUT_LOG" ]; then
			echo "Detected recovery to a single live queue segment"
			return 0
		fi

		if [ -f "$RSYSLOG_PIDBASE.pid" ] && \
		   ! ps -p "$(cat "$RSYSLOG_PIDBASE.pid" 2>/dev/null)" > /dev/null 2>&1; then
			echo "FAIL: rsyslog exited before recovery outcome was observed"
			[ -s "$RSYSLOGD_LOG" ] && cat "$RSYSLOGD_LOG"
			error_exit 1
		fi

		$TESTTOOL_DIR/msleep 100
	done

	echo "FAIL: timed out waiting for truncated queue recovery outcome"
	list_spool_top
	[ -s "$RSYSLOGD_LOG" ] && cat "$RSYSLOGD_LOG"
	error_exit 1
}

prepare_corrupted_queue

if [ ! -f "$SPOOL_DIR/mainq.qi" ]; then
	echo "FAIL: mainq.qi missing after initial shutdown"
	list_spool_top
	error_exit 1
fi

echo "spool files after initial shutdown:"
list_spool_top

corrupt_middle_segment

printf 'RSYSLOG RESTART\n\n'
write_phase2_conf

: > "$RSYSLOGD_LOG"
: > "$STARTED_LOG"
export RS_REDIR=">>\"$RSYSLOGD_LOG\" 2>&1"
bad_before=$(count_bad_dirs)

startup
wait_for_recovery_outcome "$bad_before"
shutdown_immediate
wait_shutdown "" "$SHUTDOWN_TIMEOUT"
unset RS_REDIR

echo "spool files after restart recovery:"
list_spool_top

bad_after=$(count_bad_dirs)
if [ "$bad_after" -gt "$bad_before" ]; then
	echo "truncated queue segment quarantined into mainq.bad.*"
	check_no_live_mainq_files
	exit_test
fi

if [ ! -s "$RSYSLOG_OUT_LOG" ]; then
	echo "FAIL: truncated queue recovery produced no output and no quarantine directory"
	list_spool_top
	cat "$RSYSLOGD_LOG"
	error_exit 1
fi

check_no_live_mainq_files
exit_test
