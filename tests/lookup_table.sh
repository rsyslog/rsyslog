#!/bin/bash
# added 2015-09-30 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[lookup_table_reload.sh\]: test for lookup-table and HUP based reloading of it
. $srcdir/diag.sh init
cp $srcdir/testsuites/xlate.lkp_tbl $srcdir/xlate.lkp_tbl
. $srcdir/diag.sh startup lookup_table.conf
. $srcdir/diag.sh injectmsg  0 3
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000000: foo_old"
. $srcdir/diag.sh content-check "msgnum:00000001: bar_old"
. $srcdir/diag.sh assert-content-missing "baz"
cp $srcdir/testsuites/xlate_more.lkp_tbl $srcdir/xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh injectmsg  0 3
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh content-check "msgnum:00000000: foo_new"
. $srcdir/diag.sh content-check "msgnum:00000001: bar_new"
. $srcdir/diag.sh content-check "msgnum:00000002: baz"
. $srcdir/diag.sh exit
