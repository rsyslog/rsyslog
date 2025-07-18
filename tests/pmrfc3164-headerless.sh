#!/bin/bash
# added 2025-07-18 by Codex, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="input")
parser(name="p3164" type="pmrfc3164" detect.headerless="on" headerless.hostname="n/a" headerless.tag="hdr" headerless.ruleset="hdrules")

ruleset(name="input" parser="p3164") {
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'.ok")
}
ruleset(name="hdrules") {
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
}
'

startup

tcpflood -p $TCPFLOOD_PORT -m1 -M "\"this is not syslog\""
tcpflood -p $TCPFLOOD_PORT -m1 -M "\"<13>Oct 11 22:14:15 host tag: normal\""

shutdown_when_empty
wait_shutdown

grep -q 'this is not syslog' $RSYSLOG_OUT_LOG || { cat $RSYSLOG_OUT_LOG; error_exit 1; }
grep -q 'normal' ${RSYSLOG_OUT_LOG}.ok || { cat ${RSYSLOG_OUT_LOG}.ok; error_exit 1; }

exit_test
