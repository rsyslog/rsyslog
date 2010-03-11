# This test writes to the output buffers, let's the output
# write timeout (and write data) and then continue. The conf file
# has a 2 second timeout, so we wait 4 seconds to be on the save side.
#
# added 2010-03-09 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo TEST: \[asynwr_timeout.sh\]: test async file writing timeout writes
source $srcdir/diag.sh init
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
source $srcdir/diag.sh startup asynwr_timeout.conf
# send 35555 messages, make sure file size is not a multiple of
# 10K, the buffer size!
source $srcdir/diag.sh tcpflood -m 35555
sleep 4 # wait for output writer to write and empty buffer
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
source $srcdir/diag.sh seq-check 0 35554
source $srcdir/diag.sh exit
