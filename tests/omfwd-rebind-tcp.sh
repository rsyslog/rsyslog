#!/bin/bash
# Verify that TCP rebind preserves forwarding while repeatedly tearing down
# and reconnecting the target transport.  The tiny rebind interval forces the
# rebind path for every message; the receiver sequence check proves all data
# still arrives.  When sourced by the -vg wrapper, Valgrind is the oracle for
# leaked rebind-side allocations.  This guards
# https://github.com/rsyslog/rsyslog/issues/6663.
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=40
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup

export RCVR_PORT=$TCPFLOOD_PORT
generate_conf 2
add_conf '
action(type="omfwd" target="127.0.0.1" protocol="tcp" port="'$RCVR_PORT'"
       rebindinterval="1")
' 2

startup 2
injectmsg2
shutdown_when_empty 2
wait_shutdown 2

shutdown_when_empty
wait_shutdown

seq_check
exit_test
