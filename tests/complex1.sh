# This is a rather complex test that runs a number of features together.
#
# added 2010-03-16 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[complex1.sh\]: complex test with gzip and multiple action queues
source $srcdir/diag.sh init
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout"
#export RSYSLOG_DEBUGLOG="log"
source $srcdir/diag.sh startup complex1.conf
# send 40,000 messages of 400 bytes plus header max, via three dest ports
source $srcdir/diag.sh tcpflood -m40000 -rd400 -P129 -f5 -n3 -c15 -i1
sleep 4 # due to large messages, we need this time for the tcp receiver to settle...
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
ls rsyslog.out.*.log
source $srcdir/diag.sh setzcat		   # find out which zcat to use
$ZCAT rsyslog.out.*.log > rsyslog.out.log
source $srcdir/diag.sh seq-check 1 40000 -E
source $srcdir/diag.sh exit
