#!/bin/bash
# This test hardens pmrfc3164 force.tagEndingByColon handling. The oracle is
# that a no-colon token rewinds exactly the consumed bytes, not one byte before
# the message, and a following marker message is still processed.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

parser(name="custom.rfc3164" type="pmrfc3164" force.tagEndingByColon="on")
template(name="outfmt" type="string" string="valid-after\n")

ruleset(name="testing" parser="custom.rfc3164") {
	if $rawmsg contains "valid-after" then {
		action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
	}
}'

startup
tcpflood -m1 -M "\"<14>May 21 12:00:01 host token-without-colon\""
tcpflood -m1 -M "\"<14>valid-after\""
shutdown_when_empty
wait_shutdown

export EXPECTED='valid-after'
cmp_exact
exit_test
