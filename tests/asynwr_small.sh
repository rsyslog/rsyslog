# This tests async writing with only a small set of data. That
# shall result in data staying in buffers until shutdown, what
# then will trigger some somewhat complex logic in the stream
# writer (open, write, close all during the stream close
# opertion). It is vital that only few messages be sent.
#
# The main effort of this test is not (only) to see if we
# receive the data, but rather to see if we get into an abort
# condition.
#
# added 2010-03-09 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo TEST: \[asynwr_small.sh\]: test for async file writing for few messages
source $srcdir/diag.sh init
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
source $srcdir/diag.sh startup asynwr_small.conf
# send 4000 messages
source $srcdir/diag.sh tcpflood -m2
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
source $srcdir/diag.sh seq-check 0 1
source $srcdir/diag.sh exit
