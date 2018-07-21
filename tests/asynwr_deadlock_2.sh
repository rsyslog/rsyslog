#!/bin/bash
# This is test case from practice, with the version we introduced it, it
# caused a deadlock on shutdown. I have added it to the test suite to automatically
# detect such things in the future.
#
# added 2010-03-17 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ================================================================================
echo TEST: \[asynwr_deadlock_2.sh\]: a case known to have caused a deadlock in the past
. $srcdir/diag.sh init
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
startup asynwr_deadlock_2.conf
# just send one message
. $srcdir/diag.sh tcpflood -m1
# sleep is important! need to make sure the instance is inactive
sleep 1
# now try shutdown. The actual test is if the process does hang here!
echo "processing must continue soon"
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 0
exit_test
