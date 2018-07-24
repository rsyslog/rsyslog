#!/bin/bash
# Test imtcp with many dropping connections
# added 2010-08-10 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ====================================================================================
echo TEST: \[imtcp_conndrop.sh\]: test imtcp with random connection drops
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
$OMFileFlushInterval 2
$OMFileIOBufferSize 256k
local0.* ?dynfile;outfmt
'
startup
# 100 byte messages to gain more practical data use
. $srcdir/diag.sh tcpflood -c20 -m50000 -r -d100 -P129 -D
sleep 10 # due to large messages, we need this time for the tcp receiver to settle...
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 49999 -E
exit_test
