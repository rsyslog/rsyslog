#!/bin/bash
# added 2015-09-30 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[lookup_table.sh\]: test for lookup-table and HUP based reloading of it
. $srcdir/diag.sh init
cp -f $srcdir/testsuites/xlate.lkp_tbl xlate.lkp_tbl
startup lookup_table.conf
. $srcdir/diag.sh injectmsg  0 3
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000000: foo_old"
. $srcdir/diag.sh content-check "msgnum:00000001: bar_old"
. $srcdir/diag.sh assert-content-missing "baz"
cp -f $srcdir/testsuites/xlate_more.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 3
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000000: foo_new"
. $srcdir/diag.sh content-check "msgnum:00000001: bar_new"
. $srcdir/diag.sh content-check "msgnum:00000002: baz"
cp -f $srcdir/testsuites/xlate_more_with_duplicates_and_nomatch.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 10
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
. $srcdir/diag.sh content-check "msgnum:00000000: foo_latest"
. $srcdir/diag.sh content-check "msgnum:00000001: quux"
. $srcdir/diag.sh content-check "msgnum:00000002: baz_latest"
. $srcdir/diag.sh content-check "msgnum:00000003: foo_latest"
. $srcdir/diag.sh content-check "msgnum:00000004: foo_latest"
. $srcdir/diag.sh content-check "msgnum:00000005: baz_latest"
. $srcdir/diag.sh content-check "msgnum:00000006: foo_latest"
. $srcdir/diag.sh content-check "msgnum:00000007: baz_latest"
. $srcdir/diag.sh content-check "msgnum:00000008: baz_latest"
. $srcdir/diag.sh content-check "msgnum:00000009: quux"
exit_test
