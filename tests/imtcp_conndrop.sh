#!/bin/bash
# Test imtcp with many dropping connections
# added 2010-08-10 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
. ${srcdir:=.}/diag.sh init
skip_platform "FreeBSD"  "This test currently does not work on FreeBSD"
export NUMMESSAGES=50000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
$MaxMessageSize 10k

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$template outfmt,"%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
$OMFileFlushInterval 2
$OMFileIOBufferSize 256k
local0.* ?dynfile;outfmt
'
startup
# 100 byte messages to gain more practical data use
tcpflood -c20 -m$NUMMESSAGES -r -d100 -P129 -D
shutdown_when_empty
wait_shutdown
export SEQ_CHECK_OPTIONS=-E
seq_check
exit_test
