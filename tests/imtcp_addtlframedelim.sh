#!/bin/bash
# added 2010-08-11 by Rgerhards
#
# This file is part of the rsyslog project, released  under ASL 2.0
# NOTE: we intentionally use obsolete legacy style so that we can check
# that it still works. DO NOT CONVERT/REMOVE! -- rgerhards, 2018-08-01

. $srcdir/diag.sh init
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
$InputTCPServerAddtlFrameDelimiter 0
$InputTCPServerRun 13514

$template outfmt,"%msg:F,58:2%\n"
$OMFileFlushOnTXEnd off
$OMFileFlushInterval 2
$OMFileIOBufferSize 256k'
add_conf "
local0.* ./$RSYSLOG_OUT_LOG;outfmt
"
startup
. $srcdir/diag.sh tcpflood -m20000 -F0 -P129
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 19999
exit_test
