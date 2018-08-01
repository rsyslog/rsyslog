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
# This file is part of the rsyslog project, released  under GPLv3
echo =================================================================================
echo TEST: \[asynwr_deadlock4.sh\]: a case known to have caused a deadlock in the past
. $srcdir/diag.sh init
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
$InputTCPServerRun 13514

$template outfmt,"%msg:F,58:3%,%msg:F,58:4%,%msg:F,58:5%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # use multiple dynafiles

$OMFileFlushOnTXEnd on
$OMFileFlushInterval 10
$OMFileIOBufferSize 10k
$OMFileAsyncWriting on
$DynaFileCacheSize 4
local0.* ?dynfile;outfmt
'
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
startup
# send 20000 messages, each close to 2K (non-randomized!), so that we can fill
# the buffers and hopefully run into the "deadlock".
. $srcdir/diag.sh tcpflood -m20000 -d18 -P129 -i1 -f5
# sleep is important! need to make sure the instance is inactive
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 1 20000 -E
exit_test
