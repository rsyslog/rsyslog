#!/bin/bash
# Test imptcp with many dropping connections
# added 2010-08-10 by Rgerhards
#
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=${NUMMESSAGES:-50000} # permit valgrind test to override value
export TB_TEST_MAX_RUNTIME=${TB_TEST_MAX_RUNTIME:-700} # connection drops are very slow...
generate_conf
add_conf '
$MaxMessageSize 10k

module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$template outfmt,"%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n"
template(name="dynfile" type="string" string="'$RSYSLOG_OUT_LOG'")
$OMFileFlushInterval 2
$OMFileIOBufferSize 256k
local0.* ?dynfile;outfmt
'
startup
# 100 byte messages to gain more practical data use
tcpflood -c20 -m$NUMMESSAGES -r -d100 -P129 -D
wait_file_lines
shutdown_when_empty
wait_shutdown
export SEQ_CHECK_OPTIONS=-E
seq_check
exit_test
