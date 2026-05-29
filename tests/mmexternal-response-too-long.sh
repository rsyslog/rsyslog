#!/bin/bash
# Regression coverage for bounded mmexternal replies: a helper that emits an
# oversized unterminated JSON line must be restarted before responseTimeout is
# reached, and the queue must continue to the following action. The oracle
# waits a short time for both downstream messages and checks that a second
# helper instance handled the recovery message.

. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/mmexternal/.libs/mmexternal")

template(name="outfmt" type="string" string="%msg%\n")

if $msg contains "too-long-message" then {
	action(
		type="mmexternal"
		responseTimeout="30000"
		binary="'${srcdir}'/testsuites/mmexternal-too-long-reply-bin.sh '$RSYSLOG_DYNNAME'.side '$RSYSLOG_DYNNAME'.state"
	)
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}
'

startup
injectmsg literal "<13>Mar 10 01:00:00 host tag:too-long-message-1"
injectmsg literal "<13>Mar 10 01:00:00 host tag:too-long-message-2"
wait_file_lines "$RSYSLOG_OUT_LOG" 2 5
shutdown_when_empty
wait_shutdown

content_check "too-long-message-1"
content_check "too-long-message-2"

start_count=$(grep -c '^Starting ' "$RSYSLOG_DYNNAME.side" | awk '{print $1}')
recover_count=$(grep -c '^Recovered too-long-message-2$' "$RSYSLOG_DYNNAME.side" | awk '{print $1}')

if (( start_count < 2 )); then
	echo "expected helper restart, but start count was $start_count"
	error_exit 1
fi

if (( recover_count != 1 )); then
	echo "unexpected recovery count: $recover_count"
	error_exit 1
fi

exit_test
