#!/bin/bash
# add 2016-11-22 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="customparser")
parser(name="custom.rfc3164" type="pmrfc3164" permit.AtSignsInHostname="on")
template(name="outfmt" type="string" string="-%hostname%-\n")

ruleset(name="customparser" parser="custom.rfc3164") {
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}
'
startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 Hostname1 tag: msgnum:1\""
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 Hostn@me2 tag:  msgnum:2\""
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 Hostname3 tag:msgnum:3\""
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 Hos@name4 tag4:\""
shutdown_when_empty
wait_shutdown
export EXPECTED='-Hostname1-
-Hostn@me2-
-Hostname3-
-Hos@name4-'
cmp_exact

exit_test
