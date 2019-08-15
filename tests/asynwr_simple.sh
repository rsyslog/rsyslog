#!/bin/bash
# This is test driver for testing asynchronous file output.
# This test intentionally uses legacy format.
# added 2010-03-09 by Rgerhards, re-written 2019-08-15
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
# send 35555 messages, make sure file size is not a multiple of
# 10K, the buffer size!
export NUMMESSAGES=355555
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check
generate_conf
add_conf '
$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string="'$RSYSLOG_OUT_LOG'")
$OMFileFlushOnTXEnd off
$OMFileFlushInterval 2
$OMFileIOBufferSize 10k
$OMFileAsyncWriting on
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
seq_check
exit_test
