#!/bin/bash
# added 2014-01-17 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_le.sh\]: testing rainerscript LE statement
. $srcdir/diag.sh init
. $srcdir/diag.sh startup rscript_le.conf
. $srcdir/diag.sh injectmsg  0 8000
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh seq-check  0 5000
. $srcdir/diag.sh exit
