#!/bin/bash
# Verify that pmnormalize directs enabled liblognorm tracing to its configured
# file. A non-empty trace file after parsing a message is the success oracle.
. ${srcdir:=.}/diag.sh init
require_plugin pmnormalize
DEBUG_LOG="${RSYSLOG_DYNNAME}.pmnormalize-debug.log"
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/pmnormalize/.libs/pmnormalize")

input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="normalize")

parser(name="custom.pmnormalize" type="pmnormalize" debug="on" debugFile="'$DEBUG_LOG'"
	rule="rule=:<%pri:number%> %fromhost-ip:ipv4% %hostname:word% %syslogtag:char-to:\\x3a%: %msg:rest%")

ruleset(name="normalize" parser="custom.pmnormalize") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
}
'
startup
tcpflood -m1 -M "\"<189> 127.0.0.1 ubuntu tag1: this is a test message\""
shutdown_when_empty
wait_shutdown
if [ ! -s "$RSYSLOG_OUT_LOG" ]; then
	echo "expected pmnormalize to parse the test message"
	error_exit 1
fi
if [ ! -s "$DEBUG_LOG" ]; then
	echo "expected liblognorm trace output in $DEBUG_LOG"
	error_exit 1
fi
exit_test
