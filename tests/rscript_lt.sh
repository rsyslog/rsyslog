# added 2014-01-17 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_lt.sh\]: testing rainerscript LT statement
source $srcdir/diag.sh init
source $srcdir/diag.sh startup rscript_lt.conf
source $srcdir/diag.sh injectmsg  0 8000
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh seq-check  0 4999
source $srcdir/diag.sh exit
