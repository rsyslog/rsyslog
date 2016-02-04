#!/bin/bash
# added 2016-01-20 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[multiple_lookup_table.sh\]: test for multiple lookup-table and HUP based reloading of it
. $srcdir/diag.sh init
cp $srcdir/testsuites/xlate.lkp_tbl $srcdir/xlate.lkp_tbl
cp $srcdir/testsuites/xlate.lkp_tbl $srcdir/xlate_1.lkp_tbl
. $srcdir/diag.sh startup multiple_lookup_tables.conf
. $srcdir/diag.sh injectmsg  0 3
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000000: 0_foo_old 1_foo_old"
. $srcdir/diag.sh content-check "msgnum:00000001: 0_bar_old 1_bar_old"
. $srcdir/diag.sh assert-content-missing "baz"
cp $srcdir/testsuites/xlate_more.lkp_tbl $srcdir/xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 3
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000000: 0_foo_new 1_foo_old"
. $srcdir/diag.sh content-check "msgnum:00000001: 0_bar_new 1_bar_old"
. $srcdir/diag.sh content-check "msgnum:00000002: 0_baz"
. $srcdir/diag.sh assert-content-missing "1_baz"
cp $srcdir/testsuites/xlate_more.lkp_tbl $srcdir/xlate_1.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 3
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh content-check "msgnum:00000000: 0_foo_new 1_foo_new"
. $srcdir/diag.sh content-check "msgnum:00000001: 0_bar_new 1_bar_new"
. $srcdir/diag.sh content-check "msgnum:00000002: 0_baz 1_baz"
. $srcdir/diag.sh exit
