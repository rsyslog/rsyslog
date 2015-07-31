#!/bin/bash
echo ===============================================================================
echo \[incltest_dir_wildcard.sh\]: test $IncludeConfig for directories with wildcards

. $srcdir/diag.sh init
. $srcdir/diag.sh startup incltest_dir_wildcard.conf
# 100 messages are enough - the question is if the include is read ;)
. $srcdir/diag.sh injectmsg 0 100
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 99
. $srcdir/diag.sh exit
