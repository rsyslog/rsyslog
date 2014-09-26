# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[elasticsearch-basic-bulk.sh\]: basic test for elasticsearch functionality
source $srcdir/diag.sh init
source $srcdir/diag.sh es-init
source $srcdir/diag.sh startup elasticsearch-basic-bulk.conf
source $srcdir/diag.sh injectmsg  0 10000
source $srcdir/diag.sh shutdown-when-empty
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh es-getdata 10000
source $srcdir/diag.sh seq-check  0 9999
source $srcdir/diag.sh exit
