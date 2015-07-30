#!/bin/bash
# Test for discard functionality
# This test checks if discard works. It is not a perfect test but
# will find at least segfaults and obviously not discarded messages.
# added 2009-07-30 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo ===============================================================================
echo \[discard.sh\]: testing discard functionality
. $srcdir/diag.sh init
. $srcdir/diag.sh startup discard.conf
# 20000 messages should be enough - the disk test is slow enough ;)
sleep 4
. $srcdir/diag.sh tcpflood -m10 -i1
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 2 10
. $srcdir/diag.sh exit
