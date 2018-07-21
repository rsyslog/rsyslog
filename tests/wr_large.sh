#!/bin/bash
# This tests async writing large data records. We use up to 10K
# record size.

# added 2010-03-10 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
cat rsyslog.action.1.include
. $srcdir/diag.sh init
startup wr_large.conf
# send 4000 messages of 10.000bytes plus header max, randomized
. $srcdir/diag.sh tcpflood -m4000 -r -d10000 -P129
sleep 1 # due to large messages, we need this time for the tcp receiver to settle...
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 3999 -E
exit_test
