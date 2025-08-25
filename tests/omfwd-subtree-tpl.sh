#!/bin/bash
# Verify omfwd forwards messages when using subtree-type templates
unset RSYSLOG_DYNNAME
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
$MainMsgQueueTimeoutShutdown 10000
template(name="csl-subtree" type="subtree" subtree="$!csl")

if $msg contains "msgnum:" then {
    set $!csl!foo = "bar";
    action(type="omfwd" template="csl-subtree"
           target="127.0.0.1" port="'$TCPFLOOD_PORT'" protocol="tcp")
}
'

./minitcpsrv -t127.0.0.1 -p$TCPFLOOD_PORT -f $RSYSLOG_OUT_LOG &
BGPROCESS=$!

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

content_check '{ "foo": "bar" }' "$RSYSLOG_OUT_LOG"
exit_test

