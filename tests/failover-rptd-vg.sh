#!/bin/bash
# This file is part of the rsyslog project, released under GPLv3

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[failover-rptd.sh\]: rptd test for failover functionality
. $srcdir/diag.sh init
startup_vg failover-rptd.conf
. $srcdir/diag.sh injectmsg  0 5000
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
seq_check  0 4999
exit_test
