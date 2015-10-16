#!/bin/bash
# Test for DA mode on the main message queue
# This test checks if DA mode operates correctly. To do so,
# it uses a small in-memory queue size, so that DA mode is initiated
# rather soon, and disk spooling used. There is some uncertainty (based
# on machine speeds), but in general the test should work rather well.
# We add a few messages after the initial run, just so that we can
# check everything recovers from DA mode correctly.
# added 2009-04-22 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo "[da-mainmsg-q.sh]: testing main message queue in DA mode (going to disk)"
. $srcdir/diag.sh init
. $srcdir/diag.sh startup da-mainmsg-q.conf

# part1: send first 50 messages (in memory, only)
#. $srcdir/diag.sh tcpflood 127.0.0.1 13514 1 50
. $srcdir/diag.sh injectmsg 0 50
. $srcdir/diag.sh wait-queueempty # let queue drain for this test case

# part 2: send bunch of messages. This should trigger DA mode
#. $srcdir/diag.sh injectmsg 50 20000
. $srcdir/diag.sh injectmsg 50 2000
ls -l test-spool	 # for manual review

# send another handful
. $srcdir/diag.sh injectmsg 2050 50
#sleep 1 # we need this so that rsyslogd can receive all outstanding messages

# clean up and check test result
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check  0 2099
. $srcdir/diag.sh exit
