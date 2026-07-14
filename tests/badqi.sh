#!/bin/bash
# Test startup fallback with a bad classic .qi file. The daemon must survive,
# process messages in memory, and shut down cleanly. The static fixture now
# receives a durable classic-engine marker during selection, so the test
# removes that generated marker after the daemon has stopped.
# added 2009-10-21 by RGerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo ===============================================================================
echo \[badqi.sh\]: test startup with invalid .qi file
. ${srcdir:=.}/diag.sh init
rm -f bad_qi/dbq.da-engine
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
# instruct to use bad .qi file
$WorkDirectory bad_qi
$ActionQueueType LinkedList
$ActionQueueFileName dbq
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
# we just inject a handful of messages so that we have something to wait for...
tcpflood -m20
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown  # wait for process to terminate
rm -f bad_qi/dbq.da-engine
seq_check 0 19
exit_test
