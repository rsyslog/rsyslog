#!/bin/bash
# added 2026-05-22 by Codex, released under ASL 2.0
# Regression coverage for issue #5206: responseTimeout bounds how long
# mmexternal waits for a JSON reply. A helper that never emits LF must be
# killed and restarted, and the queue must continue to the following action.
# The oracle is the configured omfile output appearing before shutdown.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/mmexternal/.libs/mmexternal")

template(name="outfmt" type="string" string="%msg%\n")

if $msg contains "timeout-message" then {
	action(type="mmexternal"
		responseTimeout="500"
		binary="'${srcdir}'/testsuites/mmexternal-hang-no-reply.sh")
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}
'

startup
injectmsg literal "<13>Mar 10 01:00:00 host tag:timeout-message"
wait_file_lines "$RSYSLOG_OUT_LOG" 1
shutdown_when_empty
wait_shutdown

content_check "timeout-message"

exit_test
