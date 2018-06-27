#!/bin/bash
# This test checks if an empty includeConfig directory causes problems. It
# should not, as this is a valid situation that by default exists on many
# distros.
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf "\$IncludeConfig ${srcdir}/testsuites/incltest.d/*.conf-not-there
"
. $srcdir/diag.sh add-conf '$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" ./rsyslog.out.log;outfmt'
. $srcdir/diag.sh startup
# 100 messages are enough - the question is if the include is read ;)
. $srcdir/diag.sh injectmsg 0 100
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 99
. $srcdir/diag.sh exit
