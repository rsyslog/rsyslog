#!/bin/bash
# added 2010-08-11 by Rgerhards
#
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=20000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp" addtlframedelimiter="0")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
local0.* ./'$RSYSLOG_OUT_LOG';outfmt
'
startup
tcpflood -m$NUMMESSAGES -F0 -P129
shutdown_when_empty
wait_shutdown
seq_check
exit_test
