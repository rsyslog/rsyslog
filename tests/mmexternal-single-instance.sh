#!/bin/bash
# This regression verifies that mmexternal forceSingleInstance="on" really
# shares one helper across many workers. The oracle checks that the helper
# writes exactly one Starting line, one Terminating line, and one Received line
# per injected message while downstream omfile still receives every message.

. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "This test currently does not work on Solaris"
export NUMMESSAGES=10000

generate_conf
add_conf '
module(load="../plugins/mmexternal/.libs/mmexternal")

template(name="outfmt" type="string" string="%msg%\n")

:msg, contains, "msgnum:" {
	action(
		type="mmexternal"
		binary="'$srcdir'/testsuites/mmexternal-single-instance-bin.sh '$RSYSLOG_DYNNAME'.side"
		forceSingleInstance="on"
		queue.type="LinkedList"
		queue.workerThreads="10"
		queue.size="5000"
		queue.timeoutShutdown="30000"
	)
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}
'

startup
injectmsg
shutdown_when_empty
wait_shutdown

start_count=$(grep -c '^Starting$' "$RSYSLOG_DYNNAME.side" | awk '{print $1}')
term_count=$(grep -c '^Terminating$' "$RSYSLOG_DYNNAME.side" | awk '{print $1}')
recv_count=$(grep -c '^Received .*msgnum:' "$RSYSLOG_DYNNAME.side" | awk '{print $1}')
line_count=$(wc -l < "$RSYSLOG_OUT_LOG" | awk '{print $1}')

if (( start_count != 1 )); then
	echo "unexpected helper start count: $start_count"
	error_exit 1
fi

if (( term_count != 1 )); then
	echo "unexpected helper termination count: $term_count"
	error_exit 1
fi

if (( recv_count != NUMMESSAGES )); then
	echo "unexpected helper receive count: $recv_count (expected: $NUMMESSAGES)"
	error_exit 1
fi

if (( line_count != NUMMESSAGES )); then
	echo "unexpected output line count: $line_count (expected: $NUMMESSAGES)"
	error_exit 1
fi

exit_test
