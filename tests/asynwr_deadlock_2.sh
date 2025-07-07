#!/bin/bash
# This is test case from practice, with the version we introduced it, it
# caused a deadlock on shutdown. I have added it to the test suite to automatically
# detect such things in the future.
# a case known to have caused a deadlock in the past
#
# added 2010-03-17 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$template outfmt,"%msg:F,58:2%\n"

$OMFileFlushOnTXEnd on
$OMFileFlushInterval 10
$OMFileIOBufferSize 4k
$OMFileAsyncWriting on
:msg, contains, "msgnum:" action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
tcpflood
wait_file_lines # wait to become inactive - important bug trigger contidion
# now try shutdown. The actual test is if the process does hang here!
echo "processing must continue soon"
shutdown_when_empty
wait_shutdown
seq_check
exit_test
