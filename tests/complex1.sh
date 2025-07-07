#!/bin/bash
# This is a rather complex test that runs a number of features together.
#
# added 2010-03-16 by Rgerhards
#
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS"  "This test currently does not work on all flavors of Solaris."
export NUMMESSAGES=40000
export RSYSLOG_PORT2="$(get_free_port)"
export RSYSLOG_PORT3="$(get_free_port)"
generate_conf
echo ports: $TCPFLOOD_PORT $RSYSLOG_PORT2 $RSYSLOG_PORT3
add_conf '
$MaxMessageSize 10k

$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000

$template outfmt,"%msg:F,58:3%,%msg:F,58:4%,%msg:F,58:5%\n"
$template dynfile,"'$RSYSLOG_DYNNAME'.out.%inputname%.%msg:F,58:2%.log.Z"

## RULESET with listener
$Ruleset R13514
# queue params:
$ActionQueueTimeoutShutdown 60000
$ActionQueueTimeoutEnqueue 20000
$ActionQueueSize 5000
$ActionQueueSaveOnShutdown on
$ActionQueueHighWaterMark 4900
$ActionQueueLowWaterMark 3500
$ActionQueueType FixedArray
$ActionQueueWorkerThreads 1
# action params:
$OMFileFlushOnTXEnd off
$OMFileZipLevel 6
$DynaFileCacheSize 4
$omfileFlushInterval 1
*.* ?dynfile;outfmt
action(type="omfile" file ="'$RSYSLOG_DYNNAME'.countfile")
# listener
$InputTCPServerInputName '$TCPFLOOD_PORT'
$InputTCPServerBindRuleset R13514
$InputTCPServerRun '$TCPFLOOD_PORT'


## RULESET with listener
$Ruleset R_PORT2
# queue params:
$ActionQueueTimeoutShutdown 60000
$ActionQueueTimeoutEnqueue 20000
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
action(type="omfile" file ="'$RSYSLOG_DYNNAME'.countfile")
# listener
$InputTCPServerInputName '$RSYSLOG_PORT2'
$InputTCPServerBindRuleset R_PORT2
$InputTCPServerRun '$RSYSLOG_PORT2'



## RULESET with listener
$Ruleset R_PORT3
# queue params:
$ActionQueueTimeoutShutdown 60000
$ActionQueueTimeoutEnqueue 20000
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
action(type="omfile" file ="'$RSYSLOG_DYNNAME'.countfile")
# listener
$InputTCPServerInputName '$RSYSLOG_PORT3'
$InputTCPServerBindRuleset R_PORT3
$InputTCPServerRun '$RSYSLOG_PORT3'
'

count_function() {
	# TODO: try to make this work on the compressed files, only
	# idea does not work as we miss end-of-zip record
	# leaving it commented out if we see we should really switch to that
	# method; if we do not need for an extended period of time, this shall
	# be removed -- rgerhards, 2018-12-19
	#mkdir "$RSYSLOG_DYNNAME.countwrk"
	#cp $RSYSLOG_DYNNAME.out.*.log.Z "$RSYSLOG_DYNNAME.countwrk"
	#cd "$RSYSLOG_DYNNAME.countwrk"
	#gunzip $RSYSLOG_DYNNAME.out.*.log.Z
	#printf '%d' $(cat $RSYSLOG_DYNNAME.out.*.log | wc -l)
	#cd ..
	#rm -rf "$RSYSLOG_DYNNAME.countwrk"

	# now the real - simple - code:
	printf '%d' $(wc -l < $RSYSLOG_DYNNAME.countfile)
}

startup
# send 40,000 messages of 400 bytes plus header max, via three dest ports
export TCPFLOOD_PORT="$TCPFLOOD_PORT:$RSYSLOG_PORT2:$RSYSLOG_PORT3"
tcpflood -m$NUMMESSAGES -rd400 -P129 -f5 -n3 -c15 -i1

# note: we have FlushOnTX=off, so we will not see the last block inside the file;
# as such we wait until a "sufficiently large" number of messages has arrived and
# hope that shutdown_when_empty gets us toward the rest. It's still a bit racy,
# but should be better than without the wait_file_lines.
wait_file_lines --delay 1000 --count-function count_function DUMMY-filename $((NUMMESSAGES - 1500))
shutdown_when_empty
wait_shutdown
ls $RSYSLOG_DYNNAME.out.*.log.Z
gunzip $RSYSLOG_DYNNAME.out.*.log.Z
cat $RSYSLOG_DYNNAME.out.*.log > $RSYSLOG_OUT_LOG
seq_check 1 $NUMMESSAGES -E
exit_test
