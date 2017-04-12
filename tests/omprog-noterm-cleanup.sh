#!/bin/bash
# added 2016-11-03 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[omprog-noterm-cleanup.sh\]: test for cleanup in omprog without SIGTERM

uname
if [ `uname` = "SunOS" ] ; then
   echo "Solaris: FIX ME"
   exit 77
fi

. $srcdir/diag.sh init
. $srcdir/diag.sh startup omprog-noterm.conf
. $srcdir/diag.sh wait-startup
. $srcdir/diag.sh injectmsg  0 5
sleep 1
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000000:"
. $srcdir/diag.sh getpid

old_fd_count=$(lsof -p $pid | wc -l)

for i in $(seq 5 10); do
    set -x
    pkill -USR1 -f omprog-noterm.sh
    set +x
    sleep .1
    . $srcdir/diag.sh injectmsg  $i 1
    sleep .5
done
sleep .5

. $srcdir/diag.sh content-check "msgnum:00000009:"

new_fd_count=$(lsof -p $pid | wc -l)
echo OLD: $old_fd_count NEW: $new_fd_count
. $srcdir/diag.sh assert-equal $old_fd_count $new_fd_count 2

echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown
sleep 1
. $srcdir/diag.sh assert-content-missing "received SIGTERM"
. $srcdir/diag.sh content-check "PROCESS TERMINATED (last msg: Exit due to read-failure)"
. $srcdir/diag.sh exit
