#!/bin/bash
# This is test driver for testing asynchronous file output.
#
# added 2010-03-09 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo TEST: \[asynwr_simple.sh\]: simple test for async file writing
. $srcdir/diag.sh init
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
startup asynwr_simple.conf
# send 35555 messages, make sure file size is not a multiple of
# 10K, the buffer size!
. $srcdir/diag.sh tcpflood -m35555
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 35554
exit_test
