#!/bin/bash
# This test hardens pmdb2diag against malformed DB2-like tails. The oracle is
# that unterminated PID/program delimiters do not trigger unbounded searches or
# NULL pointer arithmetic, and a following marker message is still processed.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../contrib/pmdb2diag/.libs/pmdb2diag")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

parser(type="pmdb2diag" timeformat="%Y-%m-%d-%H.%M.%S." timepos="0" levelpos="59" pidstarttoprogstartshift="49" name="db2.diag.hardened")
template(name="outfmt" type="string" string="valid-after\n")

ruleset(name="testing" parser="db2.diag.hardened") {
	if $rawmsg contains "valid-after" then {
		action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
	}
}'

startup
tcpflood -m1 -M "\"<14>2015-05-06-16.53.26.989430+120 E1876227378A1702     LEVEL: Info
PID     :\""
tcpflood -m1 -M "\"<14>valid-after\""
shutdown_when_empty
wait_shutdown

export EXPECTED='valid-after'
cmp_exact
exit_test
