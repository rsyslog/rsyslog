#!/bin/bash
# Test for disk-only queue mode (with fsync for queue files)
# This test checks if queue files can be correctly written
# and read back, but it does not test the transition from
# memory to disk mode for DA queues.
# added 2009-06-09 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000 # 1000 messages should be enough - the disk fsync test is very slow!
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
if [ $(uname) = "SunOS" ] ; then
   echo "This test currently does not work on all flavors of Solaris."
   exit 77
fi

generate_conf
add_conf '
# set spool locations and switch queue to disk-only mode
$WorkDirectory '$RSYSLOG_DYNNAME'.spool
$MainMsgQueueSyncQueueFiles on
$MainMsgQueueTimeoutShutdown 10000
$MainMsgQueueFilename mainq
$MainMsgQueueType disk

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
seq_check
exit_test
