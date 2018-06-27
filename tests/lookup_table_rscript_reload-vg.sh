#!/bin/bash
# added 2015-12-18 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[lookup_table_rscript_reload-vg.sh\]: test for lookup-table reload by rscript-fn with valgrind
. $srcdir/diag.sh init
cp -f $srcdir/testsuites/xlate.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh startup-vg lookup_table_reload_stub.conf
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
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh content-check "msgnum:00000000: foo_latest"
. $srcdir/diag.sh content-check "msgnum:00000001: quux"
. $srcdir/diag.sh content-check "msgnum:00000002: baz_latest"
. $srcdir/diag.sh content-check "msgnum:00000000: reload_failed"
. $srcdir/diag.sh content-check "msgnum:00000000: reload_failed"

. $srcdir/diag.sh exit
