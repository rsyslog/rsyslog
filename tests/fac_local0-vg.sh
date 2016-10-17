#!/bin/bash
# added 2016-10-14 by janmejay.singh

# This file is part of the rsyslog project, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh startup-vg fac_local0.conf
. $srcdir/diag.sh tcpflood -m1000 -P 129
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh seq-check 0 999 
. $srcdir/diag.sh exit
