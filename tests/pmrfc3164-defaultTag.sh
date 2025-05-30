#!/bin/bash
# add 2016-11-22 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="customparser")
parser(name="custom.rfc3164" type="pmrfc3164" permit.AtSignsInHostname="off"
	force.tagEndingByColon="on")
template(name="outfmt" type="string" string="?%hostname%?%syslogtag%?%msg%?\n")

ruleset(name="customparser" parser="custom.rfc3164") {
	:hostname, contains, "Hostname" action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
}
'
startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 Hostname1  msgnum:1\""
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 Hostname2   msgnum:2\""
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 Hostname3 tag msgnum:3\""
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 Hostname4 tag: msg\""
shutdown_when_empty
wait_shutdown
export EXPECTED='?Hostname1?-?  msgnum:1?
?Hostname2?-?   msgnum:2?
?Hostname3?-? tag msgnum:3?
?Hostname4?tag:? msg?'
cmp_exact

exit_test
