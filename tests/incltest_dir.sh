#!/bin/bash
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
env|grep src
echo ac_top_srcdir: $ac_top_srcdir
echo FULL ENV:
env
echo FS info:
set -x
pwd
echo "srcdir: $srcdir"
ls -l ${srcdir}/testsuites/incltest.d/
ls -l ${srcdir}/testsuites
find ../../../.. -name incltest.d
set +x
. $srcdir/diag.sh add-conf "\$IncludeConfig ${srcdir}/testsuites/incltest.d/
"
. $srcdir/diag.sh startup
# 100 messages are enough - the question is if the include is read ;)
. $srcdir/diag.sh injectmsg 0 100
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 99
. $srcdir/diag.sh exit
