#!/bin/bash
# added 2015-12-18 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[lookup_table_rscript_reload_without_stub.sh\]: test for lookup-table reload by rscript-stmt without stub
. $srcdir/diag.sh init
cp -f $srcdir/testsuites/xlate.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh startup lookup_table_reload.conf
# the last message ..002 should cause successful lookup-table reload
cp -f $srcdir/testsuites/xlate_more.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh injectmsg  0 3
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000000: foo_old"
. $srcdir/diag.sh content-check "msgnum:00000001: bar_old"
. $srcdir/diag.sh assert-content-missing "baz"
cp -f $srcdir/testsuites/xlate_more_with_duplicates_and_nomatch.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh injectmsg  0 3
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000000: foo_new"
. $srcdir/diag.sh content-check "msgnum:00000001: bar_new"
. $srcdir/diag.sh content-check "msgnum:00000002: baz"
rm -f xlate.lkp_tbl # this should lead to unsuccessful reload
. $srcdir/diag.sh injectmsg  0 3
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh injectmsg  0 2
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh content-check-with-count "msgnum:00000000: foo_latest" 2
. $srcdir/diag.sh content-check-with-count "msgnum:00000001: quux" 2
. $srcdir/diag.sh content-check-with-count "msgnum:00000002: baz_latest" 1

. $srcdir/diag.sh exit
