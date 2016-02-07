#!/bin/bash
# added 2015-10-30 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[array_lookup_table-vg.sh\]: test cleanup for array lookup-table and HUP based reloading of it
. $srcdir/diag.sh init
cp $srcdir/testsuites/xlate_array_misuse.lkp_tbl $srcdir/xlate_array.lkp_tbl
. $srcdir/diag.sh startup-vg array_lookup_table.conf
. $srcdir/diag.sh injectmsg  0 3
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh assert-content-missing "foo"
. $srcdir/diag.sh assert-content-missing "bar"
. $srcdir/diag.sh assert-content-missing "baz"
cp $srcdir/testsuites/xlate_array_more_misuse.lkp_tbl $srcdir/xlate_array.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 3
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh assert-content-missing "foo"
. $srcdir/diag.sh assert-content-missing "bar"
. $srcdir/diag.sh assert-content-missing "baz"
cp $srcdir/testsuites/xlate_array_more.lkp_tbl $srcdir/xlate_array.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 3
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh content-check "msgnum:00000000: foo_new"
. $srcdir/diag.sh content-check "msgnum:00000001: bar_new"
. $srcdir/diag.sh content-check "msgnum:00000002: baz"
. $srcdir/diag.sh exit
