#!/bin/bash
# this checks against a situation where a deadlock was caused in
# practice.
# added 2018-10-17 by Jan Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="input")

ruleset(name="input") {
    set $!tag = $app-name;
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
}
'
startup
tcpflood -p $TCPFLOOD_PORT -M "[2018-10-16 09:59:13] ping pong"
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
exit_test
