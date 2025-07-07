#!/bin/bash
# test imptcp with large connection count
# test many concurrent tcp connections
# released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=40000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
$MaxOpenFiles 2000
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" SocketBacklog="1000" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
tcpflood -c1000 -m$NUMMESSAGES
shutdown_when_empty
wait_shutdown
seq_check
exit_test
