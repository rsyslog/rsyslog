#!/bin/bash
# Backward compatibility test for imptcp with compression.mode="none".
# omfwd's legacy "@@(zN)" mode compresses each message independently and
# prefixes the compressed payload with "z"; imptcp must still allow the
# built-in legacy transport-frame decompression when no stream decompression is
# active. The oracle is sequence-complete delivery of legacy-compressed messages
# through an imptcp receiver explicitly configured for no stream decompression.
#
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf
add_conf '
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
      compression.mode="none")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
                                 file="'$RSYSLOG_OUT_LOG'")
'
startup
export RCVR_PORT=$TCPFLOOD_PORT

generate_conf 2
add_conf '
*.* @@(z5)127.0.0.1:'$RCVR_PORT'
' 2
startup 2

injectmsg2
shutdown_when_empty 2
wait_shutdown 2

wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
shutdown_when_empty
wait_shutdown

seq_check
exit_test
