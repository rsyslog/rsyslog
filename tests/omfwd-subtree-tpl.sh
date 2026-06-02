#!/bin/bash
# Verify omfwd forwards messages when using subtree-type templates. The oracle
# is the received JSON subtree payload, and the receiver must be listening
# before rsyslog starts so the single test message is not lost to startup
# timing.
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

start_minitcpsrv_ready "$RSYSLOG_OUT_LOG" "$TCPFLOOD_PORT"

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

content_check '{ "foo": "bar" }' "$RSYSLOG_OUT_LOG"
exit_test
