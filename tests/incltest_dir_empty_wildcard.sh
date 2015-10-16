#!/bin/bash
# This test checks if an empty includeConfig directory causes problems. It
# should not, as this is a valid situation that by default exists on many
# distros.
echo ===============================================================================
echo \[incltest_dir_empty_wildcard.sh\]: test $IncludeConfig for \"empty\" wildcard
. $srcdir/diag.sh init
. $srcdir/diag.sh startup incltest_dir_empty_wildcard.conf
# 100 messages are enough - the question is if the include is read ;)
. $srcdir/diag.sh injectmsg 0 100
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 99
. $srcdir/diag.sh exit
