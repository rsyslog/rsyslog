#!/bin/bash
# Regression test for oversizemsg.errorfile with imptcp truncation.
# imptcp can detect and truncate an oversized TCP frame before core
# submission sees rawmsg > maxMessageSize. The test passes only if that
# pre-submit oversize detection still writes the configured JSON errorfile.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$MaxMessageSize 128
global(processInternalMessages="on"
	oversizemsg.input.mode="truncate"
	oversizemsg.errorfile="'$RSYSLOG2_OUT_LOG'")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
msg="<120> 2011-03-01T11:22:12Z host tag: this is a way too long message that has to be truncated test1 test2 test3 test4 test5 abcdefghijklmnopqrstuvwxyz"
printf '%s %s' "${#msg}" "$msg" > $RSYSLOG_DYNNAME.inputfile
tcpflood -I $RSYSLOG_DYNNAME.inputfile
shutdown_when_empty
wait_shutdown

grep '"rawmsg":.*way too long message.*"input": "imptcp"' "$RSYSLOG2_OUT_LOG" > /dev/null
if [ $? -ne 0 ]; then
	echo
	echo "FAIL: expected oversize JSON record not found. $RSYSLOG2_OUT_LOG is:"
	cat "$RSYSLOG2_OUT_LOG"
	error_exit 1
fi

exit_test
