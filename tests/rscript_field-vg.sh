#!/bin/bash
# added 2012-09-20 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[rscript_field-vg.sh\]: testing rainerscript field\(\) function
. $srcdir/diag.sh init
startup_vg rscript_field.conf
. $srcdir/diag.sh injectmsg  0 5000
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
seq_check  0 4999
exit_test
