#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# add 2020-05-14 by alorbach, released under ASL 2.0
# Oversized LF-framed messages split into two chunks while emitting one
# operator-visible diagnostic per original frame when reporting is enabled.
# Exact chunk and diagnostic counts prove that split chunks do not repeat it.
export NUMMESSAGES=10
export TEST_BYTES_SENDSIZE=5037
export TEST_BYTES_EXPECTED=$(((TEST_BYTES_SENDSIZE/2 - 420) * NUMMESSAGES))
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(
	workDirectory="'$RSYSLOG_DYNNAME.spool'"
	maxMessageSize="4k"
	processInternalMessages="on"
	oversizemsg.report="on"
)
module(	load="../plugins/imtcp/.libs/imtcp"
	MaxSessions="10000"
	discardTruncatedMsg="off"
)
input(
	type="imtcp"
	name="imtcp"
	port="'$TCPFLOOD_PORT'"
	ruleset="print"
)

template(name="print_message" type="list"){
	constant(value="inputname: ")
	property(name="inputname")
	constant(value=", strlen: ")
	property(name="$!strlen")
	constant(value=", message: ")
	property(name="msg")
	constant(value="\n")
}
ruleset(name="print") {
	set $!strlen = strlen($msg);
	action(
		type="omfile" template="print_message"
		file=`echo $RSYSLOG_OUT_LOG`
	)
}
action(type="omfile" file="'$RSYSLOG2_OUT_LOG'")
'
startup
tcpflood -m$NUMMESSAGES -d $TEST_BYTES_SENDSIZE

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown

content_count_check --regex "inputname: imtcp, strlen:" $((NUMMESSAGES * 2))
if [ "$(grep -c 'imtcp.*received oversize.*message' "$RSYSLOG2_OUT_LOG" || true)" -ne "$NUMMESSAGES" ]; then
	echo "FAIL: expected one oversize diagnostic per LF-framed message"
	cat "$RSYSLOG2_OUT_LOG"
	error_exit 1
fi

count=$(wc -c < $RSYSLOG_OUT_LOG)
if [ $count -lt $TEST_BYTES_EXPECTED ]; then
	echo
	echo "FAIL: expected bytes count $count did not match $TEST_BYTES_EXPECTED. "
	echo
	echo "First 100 bytes of $RSYSLOG_OUT_LOG are: "
	head -c 100 $RSYSLOG_OUT_LOG
	echo 
	echo "Last 100 bytes of $RSYSLOG_OUT_LOG are: "
	tail -c 100 $RSYSLOG_OUT_LOG
	error_exit 1
else
	echo "Found $count bytes (Expected $TEST_BYTES_EXPECTED) in $RSYSLOG_OUT_LOG"
fi

exit_test
