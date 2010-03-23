# This is test case from practice, with the version we introduced it, it
# caused a deadlock during processing.
# We added this as a standard test in the hopes that iw will help
# detect such things in the future.
#
# This is a test that is constructed similar to asynwr_deadlock2.sh, but
# can produce problems in a simpler way.
#
# added 2010-03-18 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo =================================================================================
echo TEST: \[asynwr_deadlock4.sh\]: a case known to have caused a deadlock in the past
source $srcdir/diag.sh init
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
source $srcdir/diag.sh startup asynwr_deadlock4.conf
# send 20000 messages, each close to 2K (non-randomized!), so that we can fill
# the buffers and hopefully run into the "deadlock".
source $srcdir/diag.sh tcpflood -m20000 -d18 -P129 -i1 -f5
# sleep is important! need to make sure the instance is inactive
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
source $srcdir/diag.sh seq-check 1 20000 -E
source $srcdir/diag.sh exit
