#!/bin/bash
# added 2015-10-30 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[sparse_array_lookup_table.sh\]: test for sparse-array lookup-table and HUP based reloading of it
. $srcdir/diag.sh init
generate_conf
add_conf '
lookup_table(name="xlate" file="xlate_array.lkp_tbl")

template(name="outfmt" type="string" string="%msg% %$.lkp%\n")

set $.num = field($msg, 58, 2);

set $.lkp = lookup("xlate", $.num);

action(type="omfile" file="./rsyslog.out.log" template="outfmt")
'
cp -f $srcdir/testsuites/xlate_sparse_array.lkp_tbl xlate_array.lkp_tbl
startup_vg
. $srcdir/diag.sh injectmsg  0 1
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh assert-content-missing "foo"
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000001: foo_old"
. $srcdir/diag.sh content-check "msgnum:00000002: foo_old"
. $srcdir/diag.sh content-check "msgnum:00000003: bar_old"
. $srcdir/diag.sh content-check "msgnum:00000004: bar_old"
. $srcdir/diag.sh assert-content-missing "baz"
cp -f $srcdir/testsuites/xlate_sparse_array_more.lkp_tbl xlate_array.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 6
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000000: foo_new"
. $srcdir/diag.sh content-check "msgnum:00000001: foo_new"
. $srcdir/diag.sh content-check "msgnum:00000002: bar_new"
. $srcdir/diag.sh content-check "msgnum:00000003: bar_new"
. $srcdir/diag.sh content-check "msgnum:00000004: baz"
. $srcdir/diag.sh content-check "msgnum:00000005: baz"
cp -f $srcdir/testsuites/xlate_sparse_array_more_with_duplicates_and_nomatch.lkp_tbl xlate_array.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 15
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh content-check "msgnum:00000000: quux"
. $srcdir/diag.sh content-check "msgnum:00000001: quux"
. $srcdir/diag.sh content-check "msgnum:00000002: foo_latest"
. $srcdir/diag.sh content-check "msgnum:00000003: baz_latest"
. $srcdir/diag.sh content-check "msgnum:00000004: foo_latest"
. $srcdir/diag.sh content-check "msgnum:00000005: foo_latest"
. $srcdir/diag.sh content-check "msgnum:00000006: foo_latest"
. $srcdir/diag.sh content-check "msgnum:00000007: foo_latest"
. $srcdir/diag.sh content-check "msgnum:00000008: baz_latest"
. $srcdir/diag.sh content-check "msgnum:00000009: baz_latest"
. $srcdir/diag.sh content-check "msgnum:00000010: baz_latest"
. $srcdir/diag.sh content-check "msgnum:00000011: baz_latest"
. $srcdir/diag.sh content-check "msgnum:00000012: foo_latest"
. $srcdir/diag.sh content-check "msgnum:00000013: foo_latest"
. $srcdir/diag.sh content-check "msgnum:00000014: foo_latest"
exit_test
