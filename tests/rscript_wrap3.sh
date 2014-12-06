# added 2014-11-03 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_wrap3.sh\]: a test for wrap\(3\) script-function
source $srcdir/diag.sh init
source $srcdir/diag.sh startup rscript_wrap3.conf
source $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/wrap3_input
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh content-check "bcdefbcfoo says a abcESCdefb has ESCbcdefbc"
source $srcdir/diag.sh exit
