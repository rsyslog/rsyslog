#!/bin/bash
# added 2016-09-23 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[omprog-cleanup-when-unresponsive-vg.sh\]: test for cleanup in omprog when child-process is not responding with valgrind
. $srcdir/diag.sh init
. $srcdir/diag.sh startup-vg omprog-cleanup-unresponsive.conf
. $srcdir/diag.sh wait-startup
. $srcdir/diag.sh injectmsg  0 5
sleep 1
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000000:"
. $srcdir/diag.sh getpid

echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh content-check "received SIGTERM, ignoring it" #verify it actually received term
. $srcdir/diag.sh ensure-no-process-exists term-ignoring-script.sh
. $srcdir/diag.sh exit
