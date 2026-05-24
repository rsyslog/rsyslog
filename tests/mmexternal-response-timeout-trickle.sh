#!/bin/bash
# added 2026-05-24 by Cursor, released under ASL 2.0
# Regression coverage for mmexternal responseTimeout with partial helper output:
# a helper that trickles bytes but never completes the LF-framed JSON response
# must still be killed within the configured response timeout. The oracle checks
# that the downstream omfile action runs and that the helper is killed before it
# can write its delayed completion marker.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/mmexternal/.libs/mmexternal")

template(name="outfmt" type="string" string="%msg%\n")

if $msg contains "timeout-trickle-message" then {
	action(type="mmexternal"
		responseTimeout="500"
		binary="'${srcdir}'/testsuites/mmexternal-trickle-no-lf.sh '$RSYSLOG_DYNNAME'.trickle-finished")
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}
'

startup
injectmsg literal "<13>Mar 10 01:00:00 host tag:timeout-trickle-message"
wait_file_lines "$RSYSLOG_OUT_LOG" 1 5
shutdown_when_empty
wait_shutdown

content_check "timeout-trickle-message"
if [ -f "$RSYSLOG_DYNNAME.trickle-finished" ]; then
	printf 'FAIL: mmexternal helper was not killed before completing trickle output\n'
	error_exit 1
fi

exit_test
