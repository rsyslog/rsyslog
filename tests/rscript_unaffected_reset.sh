# Check if a set statement to the same subtree does not reset
# other variables in that same subtree.
# Copyright 2014-11-24 by Rainer Gerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_unaffected_reset.sh\]: testing set/reset
source $srcdir/diag.sh init
source $srcdir/diag.sh startup rscript_unaffected_reset.conf
source $srcdir/diag.sh injectmsg  0 100
source $srcdir/diag.sh shutdown-when-empty
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh seq-check  0 99
source $srcdir/diag.sh exit
