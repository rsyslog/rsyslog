#!/bin/bash
# This file is part of the rsyslog project, released under GPLv3
echo ===============================================================================
echo \[failover-double.sh\]: test for double failover functionality
. $srcdir/diag.sh init
. $srcdir/diag.sh startup failover-double.conf
. $srcdir/diag.sh injectmsg  0 5000
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh seq-check  0 4999
. $srcdir/diag.sh exit
