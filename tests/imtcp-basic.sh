#!/bin/bash
# add 2016-05-13 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="'$RSYSLOG_OUT_LOG'")
'
startup
tcpflood -m $NUMMESSAGES
shutdown_when_empty
wait_shutdown
seq_check
exit_test
