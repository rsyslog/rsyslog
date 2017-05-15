#!/bin/bash
# added 2016-11-03 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo '[omprog-noterm-default.sh]: test for cleanup in omprog without SIGTERM with default (off) signalOnClose'

uname
if [ `uname` = "SunOS" ] ; then
   echo "Solaris: FIX ME"
   exit 77
fi

. $srcdir/diag.sh init
. $srcdir/diag.sh startup omprog-noterm-default.conf
. $srcdir/diag.sh wait-startup
. $srcdir/diag.sh injectmsg  0 10
sleep 1
. $srcdir/diag.sh wait-queueempty
sleep 1
. $srcdir/diag.sh content-check "msgnum:00000009:"

echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown
sleep 1
. $srcdir/diag.sh assert-content-missing "received SIGTERM"
. $srcdir/diag.sh content-check "PROCESS TERMINATED (last msg: Exit due to read-failure)"
. $srcdir/diag.sh exit
