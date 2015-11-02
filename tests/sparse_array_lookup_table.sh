#!/bin/bash
# added 2015-10-30 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[sparse_array_lookup_table.sh\]: test for sparse-array lookup-table and HUP based reloading of it
. $srcdir/diag.sh init
cp $srcdir/testsuites/xlate_sparse_array.lkp_tbl $srcdir/xlate_array.lkp_tbl
. $srcdir/diag.sh startup array_lookup_table.conf
sleep 5
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000000: foo_old"
. $srcdir/diag.sh content-check "msgnum:00000002: bar_old"
. $srcdir/diag.sh assert-content-missing "baz"
cp $srcdir/testsuites/xlate_sparse_array_more.lkp_tbl $srcdir/xlate_array.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh injectmsg  0 5
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh content-check "msgnum:00000000: foo_new"
. $srcdir/diag.sh content-check "msgnum:00000002: bar_new"
. $srcdir/diag.sh content-check "msgnum:00000004: baz"
. $srcdir/diag.sh exit
