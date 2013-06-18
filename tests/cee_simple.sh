# added 2012-09-19 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[cee_simple.sh\]: basic CEE property test
source $srcdir/diag.sh init
source $srcdir/diag.sh startup cee_simple.conf
source $srcdir/diag.sh injectmsg  0 5000
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh seq-check  0 4999
source $srcdir/diag.sh exit
