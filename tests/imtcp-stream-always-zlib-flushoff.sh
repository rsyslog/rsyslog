#!/bin/bash
# added 2026-05-17 by Codex, released under ASL 2.0
# Verifies that zlib stream:always data is held until the omfwd stream closes
# when flushOnTXEnd is off; the receiver must emit no early partial message.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=1

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
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
	compression.stream.flushOnTXEnd="off")
' 2
startup 2

injectmsg2
sleep 1
if [ -s "$RSYSLOG_OUT_LOG" ]; then
	echo "FAIL: receiver emitted data before sender shutdown with compression.stream.flushOnTXEnd=off"
	cat "$RSYSLOG_OUT_LOG"
	error_exit 1
fi

shutdown_when_empty 2
wait_shutdown 2
wait_file_lines
shutdown_when_empty
wait_shutdown
seq_check
exit_test
