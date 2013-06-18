# This is test driver for a pipe that has no reader. This mimics a usual
# real-world scenario, the /dev/xconsole pipe. Some versions of rsyslog
# were known to hang or loop on this pipe, thus we added this scenario
# as a permanent testcase. For some details, please see bug tracker
# http://bugzilla.adiscon.com/show_bug.cgi?id=186
#
# IMPORTANT: we do NOT check any result message set. The whole point in
# this test is to verify that we do NOT run into an eternal loop. As such,
# the test is "PASS", if rsyslogd terminates. If it does not terminate, we
# obviously do not cause "FAIL", but processing will hang, which should be
# a good-enough indication of failure.
#
# added 2010-04-26 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo TEST: \[pipe_noreader.sh\]: test for pipe writing without reader
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
source $srcdir/diag.sh init
mkfifo ./rsyslog.pipe
source $srcdir/diag.sh startup pipe_noreader.conf
# we need to emit ~ 128K of data according to bug report
source $srcdir/diag.sh tcpflood -m1000 -d500
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
# NO need to check seqno -- see header comment
echo we did not loop, so the test is sucessfull
source $srcdir/diag.sh exit
