#!/bin/bash
# This test writes to the output buffers, let's the output
# write timeout (and write data) and then continue. The conf file
# has a 2 second timeout, so we wait 4 seconds to be on the save side.
#
# added 2010-03-09 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo TEST: \[asynwr_timeout.sh\]: test async file writing timeout writes
. $srcdir/diag.sh init
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
. $srcdir/diag.sh startup asynwr_timeout.conf
# send 35555 messages, make sure file size is not a multiple of
# 10K, the buffer size!
. $srcdir/diag.sh tcpflood -m 35555
sleep 4 # wait for output writer to write and empty buffer
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
. $srcdir/diag.sh seq-check 0 35554
. $srcdir/diag.sh exit
