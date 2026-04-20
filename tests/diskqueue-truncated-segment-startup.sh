#!/bin/bash
# Validate queue.onCorruption handling when bytes inside a persisted disk-queue
# segment are corrupted in the middle of the queue sequence. Restart should either
# quarantine the corrupted queue into mainq.bad.* or finish draining it
# without leaving live queue files behind.
# added 2026-04-20 by Codex, released under ASL 2.0

. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=15000
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

:omtesting:sleep 0 20000
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
	find "$SPOOL_DIR" -maxdepth 1 -mindepth 1 -printf '%f %s bytes\n' | sort
}

count_bad_dirs() {
	find "$SPOOL_DIR" -maxdepth 1 -mindepth 1 -type d -name 'mainq.bad.*' | wc -l
}

check_no_live_mainq_files() {
	local live_files

	live_files="$(find "$SPOOL_DIR" -maxdepth 1 -mindepth 1 \
		\( -type f -o -type l \) -name 'mainq*' -printf '%f\n')"
	if [ -n "$live_files" ]; then
		echo "FAIL: live mainq files remain after restart recovery"
		printf '%s\n' "$live_files"
		ls -l "$SPOOL_DIR"
		error_exit 1
	fi
}

corrupt_middle_segment() {
	local seg_files=()
	local base
	local num
	local target
	local orig_size
	local mid_idx
	local seek_offs
	local corrupt_len

	for path in "$SPOOL_DIR"/mainq.*; do
		[ -f "$path" ] || continue
		base="${path##*/}"
		num="${base#mainq.}"
		case "$num" in
		''|*[!0-9]*)
			continue
			;;
		esac
		seg_files+=("$base")
	done

	if [ "${#seg_files[@]}" -gt 0 ]; then
		IFS=$'\n' seg_files=($(printf '%s\n' "${seg_files[@]}" | sort -t. -k2,2n))
		unset IFS
	fi

	if [ "${#seg_files[@]}" -lt 3 ]; then
		printf 'FAIL: expected at least 3 queue segment files, found %d\n' "${#seg_files[@]}"
		list_spool_top
		error_exit 1
	fi

	mid_idx=$(( ${#seg_files[@]} / 2 ))
	target="$SPOOL_DIR/${seg_files[$mid_idx]}"
	orig_size=$(stat -c%s "$target")
	seek_offs=$(( orig_size / 2 ))
	corrupt_len=4096
	if [ "$seek_offs" -le 0 ] || [ "$seek_offs" -ge "$orig_size" ]; then
		printf 'FAIL: invalid corruption offset for %s (size=%s)\n' "$target" "$orig_size"
		error_exit 1
	fi
	if [ $(( seek_offs + corrupt_len )) -gt "$orig_size" ]; then
		corrupt_len=$(( orig_size - seek_offs ))
	fi

	printf 'Corrupting queue segment %s at offset %s for %s bytes\n' "$target" "$seek_offs" "$corrupt_len"
	dd if=/dev/zero of="$target" bs=1 seek="$seek_offs" count="$corrupt_len" conv=notrunc status=none
}

write_phase1_conf
startup
injectmsg 0 "$NUMMESSAGES"
shutdown_immediate
wait_shutdown

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
shutdown_when_empty
wait_shutdown
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
