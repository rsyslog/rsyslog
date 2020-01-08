#!/bin/bash
# This test writes to the output buffers, let's the output
# write timeout (and write data) and then continue. The conf file
# has a 2 second timeout, so we wait 4 seconds to be on the save side.
#
# added 2010-03-09 by Rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
# send 35555 messages, make sure file size is not a multiple of
# 10K, the buffer size!
export NUMMESSAGES=35555
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
$OMFileFlushOnTXEnd off
$OMFileFlushInterval 2
$OMFileIOBufferSize 10k
$OMFileAsyncWriting on
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
tcpflood -m $NUMMESSAGES
printf 'waiting for timeout to occur\n'
sleep 6 # GOOD SLEEP - we wait for the timeout!
printf 'timeout should now have occurred - check file state\n'
seq_check # mow everything MUST be persisted
shutdown_when_empty
wait_shutdown
seq_check # just a double-check that nothing is added twice
exit_test
