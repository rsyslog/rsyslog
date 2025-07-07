#!/bin/bash
# added 2014-09-17 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=100
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(type="string" name="outfmt" string="%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n")
if prifilt("news.*") then
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
tcpflood -m$NUMMESSAGES -P 57
shutdown_when_empty
wait_shutdown
seq_check
exit_test
