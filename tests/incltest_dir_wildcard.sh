#!/bin/bash
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf "include(file=\"${srcdir}/testsuites/incltest.d/*.conf\")
"
. $srcdir/diag.sh startup
# 100 messages are enough - the question is if the include is read ;)
. $srcdir/diag.sh injectmsg 0 100
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 99
. $srcdir/diag.sh exit
