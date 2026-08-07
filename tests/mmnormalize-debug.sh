#!/bin/bash
# Verify that mmnormalize directs enabled liblognorm tracing to its configured
# file. A non-empty trace file after a normalized message is the success oracle.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize
DEBUG_LOG="${RSYSLOG_DYNNAME}.mmnormalize-debug.log"
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmnormalize/.libs/mmnormalize")

input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="normalize")

ruleset(name="normalize") {
	action(type="mmnormalize" useRawMsg="on" debug="on" debugFile="'$DEBUG_LOG'"
		rule="rule=:%host:word% %tag:char-to:\\x3a%: no longer listening on %ip:ipv4%#%port:number%")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
}
'
startup
tcpflood -m1 -M "\"ubuntu tag1: no longer listening on 127.168.0.1#10514\""
shutdown_when_empty
wait_shutdown
if [ ! -s "$DEBUG_LOG" ]; then
	echo "expected liblognorm trace output in $DEBUG_LOG"
	error_exit 1
fi
exit_test
