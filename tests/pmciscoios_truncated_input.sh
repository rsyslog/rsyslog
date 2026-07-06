#!/bin/bash
# This test hardens pmciscoios against truncated messages after delimiter
# skips. The oracle is that malformed Cisco-looking input does not move parser
# walks past the logical message and a following marker message is processed.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/pmciscoios/.libs/pmciscoios")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

parser(name="custom.ciscoios" type="pmciscoios")
template(name="outfmt" type="string" string="valid-after\n")

ruleset(name="testing" parser="custom.ciscoios") {
	if $rawmsg contains "valid-after" then {
		action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
	}
}'

startup
tcpflood -m1 -M "\"<14>1: \""
tcpflood -m1 -M "\"<14>2: .\""
tcpflood -m1 -M "\"<14>valid-after\""
shutdown_when_empty
wait_shutdown

export EXPECTED='valid-after'
cmp_exact
exit_test
