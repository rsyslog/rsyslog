# added 2014-01-17 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_ne.sh\]: testing rainerscript NE statement
source $srcdir/diag.sh init
source $srcdir/diag.sh startup rscript_ne.conf
source $srcdir/diag.sh injectmsg  0 8000
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh seq-check  5000 5002
source $srcdir/diag.sh exit
