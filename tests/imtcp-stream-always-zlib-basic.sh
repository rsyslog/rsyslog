#!/bin/bash
# added 2026-05-17 by Codex, released under ASL 2.0
# Verifies that imtcp inflates a zlib stream:always TCP byte stream produced by
# omfwd; the oracle is a complete, ordered sequence at the receiver.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=20000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	compression.mode="stream:always"
	compression.driver="zlib")

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
	compression.driver="zlib"
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
