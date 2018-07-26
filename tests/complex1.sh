#!/bin/bash
# This is a rather complex test that runs a number of features together.
#
# added 2010-03-16 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[complex1.sh\]: complex test with gzip and multiple action queues

uname
if [ `uname` = "SunOS" ] ; then
   echo "This test currently does not work on all flavors of Solaris."
   exit 77
fi

. $srcdir/diag.sh init
generate_conf
add_conf '
$MaxMessageSize 10k

$MainMsgQueueTimeoutEnqueue 5000

$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000

$template outfmt,"%msg:F,58:3%,%msg:F,58:4%,%msg:F,58:5%\n"
$template dynfile,"rsyslog.out.%inputname%.%msg:F,58:2%.log"

## RULESET with listener
$Ruleset R13514
# queue params:
$ActionQueueTimeoutShutdown 60000
$ActionQueueTimeoutEnqueue 5000
$ActionQueueSize 5000
$ActionQueueSaveOnShutdown on
$ActionQueueHighWaterMark 4900
$ActionQueueLowWaterMark 3500
$ActionQueueType FixedArray
$ActionQueueWorkerThreads 1
# action params:
$OMFileFlushOnTXEnd off
$OMFileZipLevel 6
#$OMFileIOBufferSize 256k
$DynaFileCacheSize 4
$omfileFlushInterval 1
*.* ?dynfile;outfmt
# listener
$InputTCPServerInputName 13514
$InputTCPServerBindRuleset R13514
$InputTCPServerRun 13514


## RULESET with listener
$Ruleset R13515
# queue params:
$ActionQueueTimeoutShutdown 60000
$ActionQueueTimeoutEnqueue 5000
$ActionQueueSize 5000
$ActionQueueSaveOnShutdown on
$ActionQueueHighWaterMark 4900
$ActionQueueLowWaterMark 3500
$ActionQueueType FixedArray
$ActionQueueWorkerThreads 1
# action params:
$OMFileFlushOnTXEnd off
$OMFileZipLevel 6
$OMFileIOBufferSize 256k
$DynaFileCacheSize 4
$omfileFlushInterval 1
*.* ?dynfile;outfmt
# listener
$InputTCPServerInputName 13515
$InputTCPServerBindRuleset R13515
$InputTCPServerRun 13515



## RULESET with listener
$Ruleset R13516
# queue params:
$ActionQueueTimeoutShutdown 60000
$ActionQueueTimeoutEnqueue 5000
$ActionQueueSize 5000
$ActionQueueSaveOnShutdown on
$ActionQueueHighWaterMark 4900
$ActionQueueLowWaterMark 3500
$ActionQueueType FixedArray
$ActionQueueWorkerThreads 1
# action params:
$OMFileFlushOnTXEnd off
$OMFileZipLevel 6
$OMFileIOBufferSize 256k
$DynaFileCacheSize 4
$omfileFlushInterval 1
*.* ?dynfile;outfmt
# listener
$InputTCPServerInputName 13516
$InputTCPServerBindRuleset R13516
$InputTCPServerRun 13516
'
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout"
#export RSYSLOG_DEBUGLOG="log"
startup
# send 40,000 messages of 400 bytes plus header max, via three dest ports
. $srcdir/diag.sh tcpflood -m40000 -rd400 -P129 -f5 -n3 -c15 -i1
sleep 4 # due to large messages, we need this time for the tcp receiver to settle...
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
ls rsyslog.out.*.log
. $srcdir/diag.sh setzcat		   # find out which zcat to use
$ZCAT rsyslog.out.*.log > rsyslog.out.log
seq_check 1 40000 -E
exit_test
