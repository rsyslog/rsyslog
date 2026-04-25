#!/bin/bash
# Validate queue.onCorruption handling when bytes inside a persisted
# disk-assisted queue segment are corrupted. The DA child is a first-class disk
# queue, so it must process the valid prefix, quarantine the unread tail into
# mainq.bad.*, and leave no stale live queue files behind. A fresh
# mainq.00000001/mainq.qi pair is acceptable because resetting the DA disk
# child constructs a new live head.
# added 2026-04-25 by Codex, released under ASL 2.0

export TEST_MAX_RUNTIME=${TEST_MAX_RUNTIME:-300}

. ${srcdir:=.}/diag.sh init

BASE_NUMMESSAGES=${NUMMESSAGES:-8000}
PHASE1_SLEEP_USEC=${PHASE1_SLEEP_USEC:-5000}
PREPARE_ATTEMPTS=${PREPARE_ATTEMPTS:-3}
RECOVERY_WAIT_TIMEOUT=${RECOVERY_WAIT_TIMEOUT:-120}
SHUTDOWN_TIMEOUT=${SHUTDOWN_TIMEOUT:-60}
CORRUPT_BYTES=${CORRUPT_BYTES:-1200000}
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
	queue.type="LinkedList"
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
	queue.type="LinkedList"
	queue.filename="mainq"
	queue.maxFileSize="16k"
	queue.saveOnShutdown="off"
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

check_clean_live_mainq_state() {
	live_list="${RSYSLOG_DYNNAME}.live-mainq"
	: > "$live_list"
	for path in "$SPOOL_DIR"/mainq*; do
		[ -e "$path" ] || continue
		[ -f "$path" ] || [ -L "$path" ] || continue
		printf '%s\n' "${path##*/}" >> "$live_list"
	done
	if [ ! -s "$live_list" ]; then
		rm -f "$live_list"
		return 0
	fi

	if ! grep -Evqx 'mainq\.00000001|mainq\.qi' "$live_list"; then
		rm -f "$live_list"
		return 0
	fi

	echo "FAIL: stale live mainq files remain after restart recovery"
	cat "$live_list"
	ls -l "$SPOOL_DIR"
	rm -f "$live_list"
	error_exit 1
}

corrupt_middle_segment() {
	seg_list="${RSYSLOG_DYNNAME}.segments"
	sorted_list="${seg_list}.sorted"
	corrupted_bytes=0
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

	mid_line=2
	while [ "$mid_line" -le "$seg_count" ] && [ "$corrupted_bytes" -lt "$CORRUPT_BYTES" ]; do
		target_base=$(sed -n "${mid_line}p" "$sorted_list")
		target="$SPOOL_DIR/$target_base"
		orig_size=$(wc -c < "$target")
		printf 'Corrupting queue segment %s for %s bytes\n' "$target" "$orig_size"
		dd if=/dev/zero of="$target" bs=1 count="$orig_size" conv=notrunc \
			>/dev/null 2>&1
		corrupted_bytes=$((corrupted_bytes + orig_size))
		mid_line=$((mid_line + 1))
	done
	if [ "$corrupted_bytes" -lt "$CORRUPT_BYTES" ]; then
		printf 'FAIL: only corrupted %s bytes, expected at least %s\n' "$corrupted_bytes" "$CORRUPT_BYTES"
		rm -f "$seg_list" "$sorted_list"
		error_exit 1
	fi
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
			printf 'Prepared persisted DA queue with %s messages and %s segment files\n' \
				"$NUMMESSAGES" "$seg_count"
			return 0
		fi

		printf 'Phase 1 attempt %s produced %s segment files with %s messages; retrying\n' \
			"$attempt" "$seg_count" "$NUMMESSAGES"
		attempt=$((attempt + 1))
		current_messages=$((current_messages + BASE_NUMMESSAGES / 2))
	done

	printf 'FAIL: unable to create a persisted DA queue with at least 3 segment files after %s attempts\n' \
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

		if [ -f "$RSYSLOG_PIDBASE.pid" ] && \
		   ! ps -p "$(cat "$RSYSLOG_PIDBASE.pid" 2>/dev/null)" > /dev/null 2>&1; then
			echo "FAIL: rsyslog exited before recovery outcome was observed"
			[ -s "$RSYSLOGD_LOG" ] && cat "$RSYSLOGD_LOG"
			error_exit 1
		fi

		$TESTTOOL_DIR/msleep 100
	done

	echo "FAIL: timed out waiting for truncated DA queue recovery outcome"
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
shutdown_when_empty
wait_shutdown "" "$RECOVERY_WAIT_TIMEOUT"
unset RS_REDIR

echo "spool files after restart recovery:"
list_spool_top

content_check "disk queue corruption could not be resynchronized; quarantined unread queue tail into" "$STARTED_LOG"
content_check "skipped " "$STARTED_LOG"
if grep -q "invalid \\.qi file" "$STARTED_LOG" "$RSYSLOGD_LOG"; then
	echo "FAIL: runtime corruption handling fell back to invalid .qi recovery"
	grep "invalid \\.qi file" "$STARTED_LOG" "$RSYSLOGD_LOG"
	error_exit 1
fi

if [ ! -s "$RSYSLOG_OUT_LOG" ]; then
	echo "FAIL: truncated DA queue recovery produced no output before quarantine"
	list_spool_top
	cat "$RSYSLOGD_LOG"
	error_exit 1
fi

bad_after=$(count_bad_dirs)
if [ "$bad_after" -le "$bad_before" ]; then
	echo "FAIL: truncated DA queue segment was not quarantined into mainq.bad.*"
	list_spool_top
	error_exit 1
fi

check_clean_live_mainq_state
exit_test
