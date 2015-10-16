#!/bin/bash
# added 2012-10-29 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_ruleset_call.sh\]: testing rainerscript ruleset\(\) and call statement
. $srcdir/diag.sh init
. $srcdir/diag.sh startup rscript_ruleset_call.conf
. $srcdir/diag.sh injectmsg  0 5000
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh seq-check  0 4999
. $srcdir/diag.sh exit
