#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[es-basic-ha.sh\]: basic test for elasticsearch high availability functionality
. $srcdir/diag.sh init
. $srcdir/diag.sh es-init
. $srcdir/diag.sh startup-vg es-basic-ha.conf
. $srcdir/diag.sh injectmsg  0 100
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh wait-for-stats-flush 'rsyslog.out.stats.log'
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh es-getdata 100
. $srcdir/diag.sh seq-check  0 99
# The configuration makes every other request from message #3 fail checkConn (N/2-1)
. $srcdir/diag.sh custom-content-check '"failed.checkConn": 49' 'rsyslog.out.stats.log'
. $srcdir/diag.sh exit
