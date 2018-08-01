#!/bin/bash
# This tests writing large data records in gzip mode. We also write it to
# 5 different dynafiles, with a dynafile cache size set to 4. So this stresses
# both the input side, as well as zip writing, async writing and the dynafile
# cache logic.
#
# This test is a bit timing-dependent on the tcp reception side, so if it fails
# one may look into the timing first. The main issue is that the testbench
# currently has no good way to know if the tcp receiver is finished. This is NOT
# a problem in rsyslogd, but only of the testbench.
#
# Note that we do not yet have sufficient support for dynafiles in diag.sh,
# so we mangle some files here manually.
#
# added 2010-03-10 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[gzipwr_large_dynfile.sh\]: test for gzip file writing for large message sets
. $srcdir/diag.sh init
generate_conf
add_conf '
$MaxMessageSize 10k

$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
$InputTCPServerRun 13514

$template outfmt,"%msg:F,58:3%,%msg:F,58:4%,%msg:F,58:5%\n"
$template dynfile,"rsyslog.out.%msg:F,58:2%.log" # use multiple dynafiles
$OMFileFlushOnTXEnd off
$OMFileZipLevel 6
$OMFileIOBufferSize 256k
$DynaFileCacheSize 4
$omfileFlushInterval 1
local0.* ?dynfile;outfmt
'
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout"
#export RSYSLOG_DEBUGLOG="log"
startup
# send 4000 messages of 10.000bytes plus header max, randomized
. $srcdir/diag.sh tcpflood -m4000 -r -d10000 -P129 -f5
sleep 2 # due to large messages, we need this time for the tcp receiver to settle...
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
gunzip < rsyslog.out.0.log > $RSYSLOG_OUT_LOG
gunzip < rsyslog.out.1.log >> $RSYSLOG_OUT_LOG
gunzip < rsyslog.out.2.log >> $RSYSLOG_OUT_LOG
gunzip < rsyslog.out.3.log >> $RSYSLOG_OUT_LOG
gunzip < rsyslog.out.4.log >> $RSYSLOG_OUT_LOG
#cat rsyslog.out.* > $RSYSLOG_OUT_LOG
seq_check 0 3999 -E
exit_test
