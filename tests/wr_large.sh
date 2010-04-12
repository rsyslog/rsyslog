# This tests async writing large data records. We use up to 10K
# record size.

# added 2010-03-10 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
cat rsyslog.action.1.include
source $srcdir/diag.sh init
source $srcdir/diag.sh startup wr_large.conf
# send 4000 messages of 10.000bytes plus header max, randomized
source $srcdir/diag.sh tcpflood -m4000 -r -d10000 -P129
sleep 1 # due to large messages, we need this time for the tcp receiver to settle...
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
source $srcdir/diag.sh seq-check 0 3999 -E
source $srcdir/diag.sh exit
