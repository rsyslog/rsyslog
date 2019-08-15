#!/bin/bash
# This test checks if more than one port file names work correctly for
# imtcp. See also:
# https://github.com/rsyslog/rsyslog/issues/3817
# added 2019-08-14 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.rcvr_port"
	ruleset="rs1")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.rcvr_port2"
	ruleset="rs2")

ruleset(name="rs1") {
	action(type="omfile" file="'$RSYSLOG_DYNNAME.1.log'" template="outfmt")
}

ruleset(name="rs2") {
	action(type="omfile" file="'$RSYSLOG_DYNNAME.2.log'" template="outfmt")
}
'
startup
assign_file_content RCVR_PORT "$RSYSLOG_DYNNAME.rcvr_port"
assign_file_content RCVR_PORT2 "$RSYSLOG_DYNNAME.rcvr_port2"
./tcpflood -p $RCVR_PORT -m10 
./tcpflood -p $RCVR_PORT2 -m10 -i10
shutdown_when_empty
wait_shutdown
printf 'checking receiver 1\n'
export SEQ_CHECK_FILE="$RSYSLOG_DYNNAME.1.log"
seq_check 0 9
printf 'checking receiver 2\n'
export SEQ_CHECK_FILE="$RSYSLOG_DYNNAME.2.log"
seq_check 10 19
exit_test
