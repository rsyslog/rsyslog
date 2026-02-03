#!/bin/bash
# Validate queue.onCorruption behavior when a middle disk-queue segment
# is missing at startup.
# added 2026-02-07 by AI-assisted development, released under ASL 2.0

. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=15000
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"
RSYSLOGD_LOG="${RSYSLOG_DYNNAME}.rsyslogd.log"
STARTED_LOG="${RSYSLOG_DYNNAME}.started"
export RS_REDIR=">>$RSYSLOGD_LOG 2>&1"

prepare_conf() {
	local mode="$1"
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
	queue.onCorruption="'"$mode"'"
)

	template(name="outfmt" type="string" string="%msg:F,58:2%\\n")
	template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`)

:omtesting:sleep 0 20000
if ($msg contains "msgnum:") then {
	action(type="omfile" template="outfmt" dynaFile="dynfile")
}
'
}

create_missing_middle_segment() {
	local missing_file
	local mid_idx
	local seg_files=()

	startup
	injectmsg 0 "$NUMMESSAGES"
	shutdown_immediate
	wait_shutdown

	mapfile -t seg_files < <(find "$SPOOL_DIR" -maxdepth 1 -type f -name 'mainq.*' -printf '%f\n' \
		| awk '/^mainq\.[0-9]+$/' | sort -t. -k2,2n)

	if [ "${#seg_files[@]}" -lt 3 ]; then
		printf 'FAIL: expected at least 3 queue segment files, found %d\n' "${#seg_files[@]}"
		ls -l "$SPOOL_DIR"
		error_exit 1
	fi

	mid_idx=$(( ${#seg_files[@]} / 2 ))
	missing_file="$SPOOL_DIR/${seg_files[$mid_idx]}"
	printf 'Removing queue segment to simulate corruption: %s\n' "$missing_file"
	rm -f -- "$missing_file"
	check_file_not_exists "$missing_file"
}

wait_shutdown_or_kill() {
	local timeout_sec="${1:-${TB_TEST_TIMEOUT:-90}}"
	local instance="${2:-}"
	local pid_file="$RSYSLOG_PIDBASE${instance}.pid.save"
	local pid
	local deadline
	local timed_out=0

	pid="$(cat "$pid_file" 2>/dev/null || true)"
	if [ -z "$pid" ]; then
		return 0
	fi

	deadline=$(( $(date +%s) + timeout_sec ))
	while kill -0 "$pid" 2>/dev/null; do
		if [ "$(date +%s)" -gt "$deadline" ]; then
			printf 'FAIL: shutdown timeout in ignore mode (pid %s), forcing kill -9\n' "$pid"
			kill -9 "$pid" 2>/dev/null || true
			timed_out=1
			break
		fi
		$TESTTOOL_DIR/msleep 200
	done

	if [ "$timed_out" -eq 1 ]; then
		error_exit 1
	fi
}

run_mode_check() {
	local mode="$1"
	local bad_before
	local bad_after

	printf '\n===== queue.onCorruption=%s =====\n' "$mode"
	rm -rf "$SPOOL_DIR" "$RSYSLOG_OUT_LOG" "$RSYSLOGD_LOG"

	prepare_conf "$mode"
	create_missing_middle_segment

	bad_before=$(find "$SPOOL_DIR" -maxdepth 1 -mindepth 1 -type d -name 'mainq.bad.*' | wc -l)
	: > "$RSYSLOGD_LOG"
	: > "$STARTED_LOG"
	rm -f "${RSYSLOG_DYNNAME}.started" "${RSYSLOG_DYNNAME}.imdiag.port" "${RSYSLOG_DYNNAME}:.pid"
	: > "$STARTED_LOG"

	startup
	$TESTTOOL_DIR/msleep 500
	shutdown_immediate
	if [ "$mode" = "ignore" ]; then
		wait_shutdown_or_kill
	else
		wait_shutdown
	fi

	case "$mode" in
	safe)
		content_check "queue corruption: missing file in sequence:" "$RSYSLOGD_LOG"
		check_not_present "pure in-memory emergency mode" "$STARTED_LOG"
		bad_after=$(find "$SPOOL_DIR" -maxdepth 1 -mindepth 1 -type d -name 'mainq.bad.*' | wc -l)
		if [ "$bad_after" -le "$bad_before" ]; then
			printf 'FAIL: expected a new mainq.bad.* directory in safe mode\n'
			ls -l "$SPOOL_DIR"
			error_exit 1
		fi
		;;
	inMemory)
		content_check "Queue corruption detected. Entering emergency in-memory mode" "$STARTED_LOG"
		content_check "pure in-memory emergency mode" "$STARTED_LOG"
		;;
	ignore)
		check_not_present "Queue corruption detected" "$STARTED_LOG"
		check_not_present "queue corruption:" "$STARTED_LOG"
		;;
	*)
		printf 'FAIL: unknown mode %s\n' "$mode"
		error_exit 1
		;;
	esac
}

run_mode_check safe
run_mode_check inMemory
run_mode_check ignore

exit_test
