# This tests async writing large data records. We use up to 10K
# record size.

# added 2010-03-10 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo TEST: \[wr_large_async.sh\]: test for file writing for large message sets
source $srcdir/diag.sh init
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
echo "\$OMFileAsyncWriting on" > rsyslog.action.1.include
source $srcdir/wr_large.sh
