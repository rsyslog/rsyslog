#!/bin/bash
# Verify omfwd accepts tcp_user_timeout and still forwards over TCP. The oracle
# is the message received by minitcpsrvr, proving the configured action wrote to
# its destination after the socket option path was exercised.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
$MainMsgQueueTimeoutShutdown 10000
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum:" then
	action(type="omfwd" template="outfmt"
	       target="127.0.0.1" port="'$TCPFLOOD_PORT'" protocol="tcp"
	       tcp_user_timeout="10000")
'

start_minitcpsrv_ready "$RSYSLOG_OUT_LOG" "$TCPFLOOD_PORT"
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

seq_check 0 0
exit_test
