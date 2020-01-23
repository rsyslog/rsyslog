#!/bin/bash
# This is test case from practice, with the version we introduced it, it
# caused a deadlock during processing.
# We added this as a standard test in the hopes that iw will help
# detect such things in the future.
#
# This is a test that is constructed similar to asynwr_deadlock2.sh, but
# can produce problems in a simpler way.
#
# added 2010-03-18 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=20000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
$InputTCPServerListenPortFile '$RSYSLOG_DYNNAME'.tcpflood_port
$InputTCPServerRun 0

$template outfmt,"%msg:F,58:3%,%msg:F,58:4%,%msg:F,58:5%\n"
template(name="dynfile" type="string" string="'$RSYSLOG_OUT_LOG'")

$OMFileFlushOnTXEnd on
$OMFileFlushInterval 10
$OMFileIOBufferSize 10k
$OMFileAsyncWriting on
$DynaFileCacheSize 4
local0.* ?dynfile;outfmt
'
startup
# send 20000 messages, each close to 2K (non-randomized!), so that we can fill
# the buffers and hopefully run into the "deadlock".
tcpflood -m20000 -d18 -P129 -f5
shutdown_when_empty
wait_shutdown
export SEQ_CHECK_OPTIONS=-E
seq_check
exit_test
