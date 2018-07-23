#!/bin/bash
# added 2016-10-14 by janmejay.singh

# This file is part of the rsyslog project, released under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

. $srcdir/diag.sh init
startup_vg fac_local0.conf
. $srcdir/diag.sh tcpflood -m1000 -P 129
shutdown_when_empty
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
seq_check 0 999 
exit_test
