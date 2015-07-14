# added 2015-06-22 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_random.sh\]: test for random-number-generator script-function
source $srcdir/diag.sh init
source $srcdir/diag.sh startup rscript_random.conf
source $srcdir/diag.sh tcpflood -m 20
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh content-pattern-check "^[0-9]$"
source $srcdir/diag.sh exit
