#!/bin/bash
# This tests async writing with only a small set of data. That
# shall result in data staying in buffers until shutdown, what
# then will trigger some somewhat complex logic in the stream
# writer (open, write, close all during the stream close
# operation). It is vital that only few messages be sent.
#
# The main effort of this test is not (only) to see if we
# receive the data, but rather to see if we get into an abort
# condition.
#
# added 2010-03-09 by Rgerhards
#
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=2
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
$InputTCPServerListenPortFile '$RSYSLOG_DYNNAME'.tcpflood_port
$InputTCPServerRun 0

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
$OMFileFlushOnTXEnd off
$OMFileFlushInterval 2
$OMFileAsyncWriting on
:msg, contains, "msgnum:" ?dynfile;outfmt
'
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
startup
tcpflood
shutdown_when_empty
wait_shutdown
exit_test
