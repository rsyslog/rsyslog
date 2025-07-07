#!/bin/bash
# added 2019-04-10 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/pmnormalize/.libs/pmnormalize")

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="ruleset")
parser(name="custom.pmnormalize" type="pmnormalize" undefinedPropertyError="on"
	rule="rule=:<%pri:number%> %fromhost-ip:ipv4% %hostname:word% %syslogtag:char-to:\\x3a%: %msg:rest%")

template(name="test" type="string" string="%msg%\n")

ruleset(name="ruleset" parser="custom.pmnormalize") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="test")
}
action(type="omfile" file="'$RSYSLOG_DYNNAME'.othermsg")
'
startup
tcpflood -m1 -M "\"<abc> 127.0.0.1 ubuntu tag1: this is a test message\""
shutdown_when_empty
wait_shutdown
export EXPECTED='<abc> 127.0.0.1 ubuntu tag1: this is a test message'
cmp_exact
content_check --regex "error .* during ln_normalize" $RSYSLOG_DYNNAME.othermsg

exit_test
