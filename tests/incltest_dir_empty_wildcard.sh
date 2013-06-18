# This test checks if an empty includeConfig directory causes problems. It
# should not, as this is a valid situation that by default exists on many
# distros.
echo ===============================================================================
echo \[incltest_dir_empty_wildcard.sh\]: test $IncludeConfig for \"empty\" wildcard
source $srcdir/diag.sh init
source $srcdir/diag.sh startup incltest_dir_empty_wildcard.conf
# 100 messages are enough - the question is if the include is read ;)
source $srcdir/diag.sh injectmsg 0 100
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 0 99
source $srcdir/diag.sh exit
