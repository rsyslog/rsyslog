#!/bin/bash
# This test hardens pmsnare against Snare tag matches without a following tab
# representation. The oracle is that short tails do not underflow rewrite
# lengths and a following marker message is still processed.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../contrib/pmsnare/.libs/pmsnare")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

template(name="outfmt" type="string" string="valid-after\n")

ruleset(name="testing" parser=["rsyslog.snare", "rsyslog.rfc3164"]) {
	if $rawmsg contains "valid-after" then {
		action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
	}
}'

startup
tcpflood -m1 -M "\"<14>May 21 12:00:01 host MSWinEventLog\""
tcpflood -m1 -M "\"<14>May 21 12:00:01 host LinuxKAudit\""
tcpflood -m1 -M "\"<14>host	MSWinEventLog\""
tcpflood -m1 -M "\"<14>valid-after\""
shutdown_when_empty
wait_shutdown

export EXPECTED='valid-after'
cmp_exact
exit_test
