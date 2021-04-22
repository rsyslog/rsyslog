#!/bin/bash
# Test for multiple ports in imtcp
# This test checks if multiple tcp listener ports are correctly
# handled by imtcp
# added 2009-05-22 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=30000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" name="i1")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port2" name="i2")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port3" name="i3")

$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_tcpflood_port2 "$RSYSLOG_DYNNAME.tcpflood_port2"
assign_rs_port "$RSYSLOG_DYNNAME.tcpflood_port3"
tcpflood -p$TCPFLOOD_PORT -m10000
tcpflood -p$TCPFLOOD_PORT2 -i10000 -m10000
tcpflood -p$RS_PORT -i20000 -m10000
shutdown_when_empty
wait_shutdown
seq_check
exit_test
