#!/bin/bash
# Test for disk-only queue mode
# This test checks if queue files can be correctly written
# and read back, but it does not test the transition from
# memory to disk mode for DA queues.
# added 2009-04-17 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. $srcdir/diag.sh init
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
$InputTCPServerRun '$TCPFLOOD_PORT'

# set spool locations and switch queue to disk-only mode
$WorkDirectory '$RSYSLOG_DYNNAME'.spool
$MainMsgQueueFilename mainq
$MainMsgQueueType disk

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
if ($msg contains "msgnum:") then
	?dynfile;outfmt
else
	action(type="omfile" file="'$RSYSLOG_DYNNAME.syslog.log'")
'
startup
# 20000 messages should be enough - the disk test is slow enough ;)
tcpflood -m20000
shutdown_when_empty
wait_shutdown
seq_check 0 19999
check_not_present "spool.* open error" $RSYSLOG_DYNNAME.syslog.log
exit_test
