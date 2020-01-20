#!/bin/bash
# This tests async writing large data records. We use up to 10K
# record size.
# added 2010-03-10 by Rgerhards
#
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=4000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
$MaxMessageSize 10k

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$template outfmt,"%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n"
template(name="dynfile" type="string" string="'$RSYSLOG_OUT_LOG'") # trick to use relative path names!
$OMFileFlushInterval 2
$OMFileIOBufferSize 256k
$OMFileAsyncWriting on
local0.* ?dynfile;outfmt
'
startup
# send 4000 messages of 10.000bytes plus header max, randomized
tcpflood -m$NUMMESSAGES -r -d10000 -P129
shutdown_when_empty
wait_shutdown
export SEQ_CHECK_OPTIONS=-E
seq_check
exit_test
