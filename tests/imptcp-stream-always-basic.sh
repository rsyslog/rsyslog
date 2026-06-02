#!/bin/bash
# added 2026-04-20 by Codex, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=20000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf
add_conf '
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
export RCVR_PORT=$TCPFLOOD_PORT

generate_conf 2
add_conf '
module(load="builtin:omfwd")

*.* action(type="omfwd"
	target="127.0.0.1"
	port="'$RCVR_PORT'"
	protocol="tcp"
	compression.mode="stream:always"
	compression.stream.flushOnTXEnd="on")
' 2
startup 2

injectmsg2
shutdown_when_empty 2
wait_shutdown 2
wait_file_lines
shutdown_when_empty
wait_shutdown
seq_check
exit_test
