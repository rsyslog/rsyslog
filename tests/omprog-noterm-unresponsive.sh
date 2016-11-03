#!/bin/bash
# added 2016-11-03 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[omprog-noterm-unresponsive.sh\]: ensure omprog script is cleanly orphaned when unresponsive if signalOnClose is disabled
. $srcdir/diag.sh init
. $srcdir/diag.sh startup omprog-noterm-unresponsive.conf
. $srcdir/diag.sh wait-startup
. $srcdir/diag.sh injectmsg  0 10
. $srcdir/diag.sh wait-queueempty
sleep 1
. $srcdir/diag.sh content-check "msgnum:00000009:"

echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown
sleep 6
. $srcdir/diag.sh assert-content-missing "received SIGTERM"
. $srcdir/diag.sh assert-content-missing "PROCESS TERMINATED"
pkill -USR1 omprog-test-bin
sleep 1
. $srcdir/diag.sh content-check "PROCESS TERMINATED"
. $srcdir/diag.sh exit
