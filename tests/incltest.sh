echo ===============================================================================
echo \[incltest.sh\]: test $IncludeConfig for specific file

source $srcdir/diag.sh init
source $srcdir/diag.sh startup incltest.conf
# 100 messages are enough - the question is if the include is read ;)
source $srcdir/diag.sh injectmsg 0 100
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 0 99
source $srcdir/diag.sh exit
