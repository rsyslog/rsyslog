#!/bin/bash
# added 2016-09-09 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[omprog-cleanup-with-outfile.sh\]: test for cleanup in omprog when used with outfile
. $srcdir/diag.sh init
. $srcdir/diag.sh startup omprog-cleanup-outfile.conf
. $srcdir/diag.sh wait-startup
. $srcdir/diag.sh injectmsg  0 5
sleep 1
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000000:"
. $srcdir/diag.sh getpid

old_fd_count=$(lsof -p $pid | wc -l)

for i in $(seq 5 100); do
    pkill -USR1 omprog-test-bin
    . $srcdir/diag.sh injectmsg  $i 1
done
sleep 1

. $srcdir/diag.sh content-check "msgnum:00000099:"

new_fd_count=$(lsof -p $pid | wc -l)
echo OLD: $old_fd_count NEW: $new_fd_count
. $srcdir/diag.sh assert-equal $old_fd_count $new_fd_count 2

echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh exit
