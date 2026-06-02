#!/bin/bash
# Regression coverage for bounded mmexternal replies: a helper that emits an
# oversized unterminated JSON line must be restarted before responseTimeout is
# reached, and the queue must continue to the following action. The oracle uses
# the helper side file to observe the oversized-reply and recovery milestones,
# then checks that both messages reached the downstream action and that a second
# helper instance handled the recovery message.

. ${srcdir:=.}/diag.sh init

wait_side_content() {
	local needle="$1"
	local timeout="${2:-${TB_TEST_TIMEOUT:-60}}"
	local deadline=$(( $(date +%s) + timeout ))
	local sidefile="$RSYSLOG_DYNNAME.side"

	while true; do
		if [ -f "$sidefile" ] && grep -Fq -- "$needle" "$sidefile"; then
			printf 'side-file milestone reached: %s\n' "$needle"
			return
		fi
		if [ "$(date +%s)" -ge "$deadline" ]; then
			printf 'FAIL: side-file milestone not reached before timeout: %s\n' "$needle"
			if [ -f "$sidefile" ]; then
				printf 'side-file content:\n'
				cat -n "$sidefile"
			else
				printf 'side-file %s does not exist\n' "$sidefile"
			fi
			error_exit 1
		fi
		$TESTTOOL_DIR/msleep 200
	done
}

generate_conf
add_conf '
module(load="../plugins/mmexternal/.libs/mmexternal")

template(name="outfmt" type="string" string="%msg%\n")

if $msg contains "too-long-message" then {
	action(
		type="mmexternal"
		# 30000 ms (30 s): long enough for helper restart/recovery to occur
		# before timing out, which keeps this regression test deterministic.
		responseTimeout="30000"
		binary="'${srcdir}'/testsuites/mmexternal-too-long-reply-bin.sh '$RSYSLOG_DYNNAME'.side '$RSYSLOG_DYNNAME'.state"
	)
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}
'

startup
injectmsg literal "<13>Mar 10 01:00:00 host tag:too-long-message-1"
wait_side_content "Oversized too-long-message-1"
injectmsg literal "<13>Mar 10 01:00:00 host tag:too-long-message-2"
wait_side_content "Recovered too-long-message-2"
wait_file_lines "$RSYSLOG_OUT_LOG" 2
shutdown_when_empty
wait_shutdown

content_check "too-long-message-1"
content_check "too-long-message-2"

start_count=$(grep -c '^Starting ' "$RSYSLOG_DYNNAME.side" || true)
recover_count=$(grep -c '^Recovered too-long-message-2$' "$RSYSLOG_DYNNAME.side" || true)

if (( start_count < 2 )); then
	echo "expected helper restart, but start count was $start_count"
	error_exit 1
fi

if (( recover_count != 1 )); then
	echo "unexpected recovery count: $recover_count"
	error_exit 1
fi

exit_test
