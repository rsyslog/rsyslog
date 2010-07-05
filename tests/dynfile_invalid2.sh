# This test checks if omfile segfaults when a file open() in dynacache mode fails.
# The test is mimiced after a real-life scenario (which, of course, was much more
# complex).
#
# added 2010-03-22 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo TEST: \[dynfile_invalid2.sh\]: test open fail for dynafiles
source $srcdir/diag.sh init
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
source $srcdir/diag.sh startup dynfile_invalid2.conf
# we send handcrafted message. We have a dynafile cache of 4, and now send one message
# each to fill up the cache.
./tcpflood -m1 -M "<129>Mar 10 01:00:00 172.20.245.8 tag msg:rsyslog.out.0.log:0"
./tcpflood -m1 -M "<129>Mar 10 01:00:00 172.20.245.8 tag msg:rsyslog.out.1.log:1"
./tcpflood -m1 -M "<129>Mar 10 01:00:00 172.20.245.8 tag msg:rsyslog.out.2.log:2"
./tcpflood -m1 -M "<129>Mar 10 01:00:00 172.20.245.8 tag msg:rsyslog.out.3.log:3"
# the next one has caused a segfault in practice
# note that /proc/rsyslog.error.file must not be creatable
./tcpflood -m1 -M "<129>Mar 10 01:00:00 172.20.245.8 tag msg:/proc/rsyslog.error.file:boom"
# some more writes
./tcpflood -m1 -M "<129>Mar 10 01:00:00 172.20.245.8 tag msg:rsyslog.out.0.log:4"
./tcpflood -m1 -M "<129>Mar 10 01:00:00 172.20.245.8 tag msg:rsyslog.out.1.log:5"
./tcpflood -m1 -M "<129>Mar 10 01:00:00 172.20.245.8 tag msg:rsyslog.out.2.log:6"
./tcpflood -m1 -M "<129>Mar 10 01:00:00 172.20.245.8 tag msg:rsyslog.out.3.log:7"
# done message generation
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
cat rsyslog.out.*.log > rsyslog.out.log
source $srcdir/diag.sh seq-check 0 7
source $srcdir/diag.sh exit
