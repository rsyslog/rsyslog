# This tests writing large data records in gzip mode. We also write it to
# 5 different dynafiles, with a dynafile cache size set to 4. So this stresses
# both the input side, as well as zip writing, async writing and the dynafile
# cache logic.
#
# This test is a bit timing-dependent on the tcp reception side, so if it fails
# one may look into the timing first. The main issue is that the testbench
# currently has no good way to know if the tcp receiver is finished. This is NOT
# a problem in rsyslogd, but only of the testbench.
#
# Note that we do not yet have sufficient support for dynafiles in diag.sh,
# so we mangle some files here manually.
#
# added 2010-03-10 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[gzipwr_large_dynfile.sh\]: test for gzip file writing for large message sets
source $srcdir/diag.sh init
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout"
#export RSYSLOG_DEBUGLOG="log"
source $srcdir/diag.sh startup gzipwr_large_dynfile.conf
# send 4000 messages of 10.000bytes plus header max, randomized
source $srcdir/diag.sh tcpflood -m4000 -r -d10000 -P129 -f5
sleep 2 # due to large messages, we need this time for the tcp receiver to settle...
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
gunzip < rsyslog.out.0.log > rsyslog.out.log
gunzip < rsyslog.out.1.log >> rsyslog.out.log
gunzip < rsyslog.out.2.log >> rsyslog.out.log
gunzip < rsyslog.out.3.log >> rsyslog.out.log
gunzip < rsyslog.out.4.log >> rsyslog.out.log
#cat rsyslog.out.* > rsyslog.out.log
source $srcdir/diag.sh seq-check 0 3999 -E
source $srcdir/diag.sh exit
