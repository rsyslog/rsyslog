#!/bin/bash
# This tests writing large data records in gzip mode. We use up to 10K
# record size.
#
# added 2010-03-10 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo TEST: \[gzipwr_large.sh\]: test for gzip file writing for large message sets
. $srcdir/diag.sh init
generate_conf
add_conf '
$MaxMessageSize 10k

$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
$InputTCPServerRun 13514

$template outfmt,"%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n"
$template dynfile,"rsyslog.out.log" # trick to use relative path names!
$OMFileFlushOnTXEnd off
$OMFileZipLevel 6
$OMFileIOBufferSize 256k
local0.* ?dynfile;outfmt
'
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
startup
# send 4000 messages of 10.000bytes plus header max, randomized
. $srcdir/diag.sh tcpflood -m4000 -r -d10000 -P129
sleep 1 # due to large messages, we need this time for the tcp receiver to settle...
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
gzip_seq_check 0 3999 -E
exit_test
