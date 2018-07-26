#!/bin/bash
# added 2015-12-18 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[lookup_table_rscript_reload.sh\]: test for lookup-table reload by rscript-fn
. $srcdir/diag.sh init
generate_conf
add_conf '
lookup_table(name="xlate" file="xlate.lkp_tbl")

template(name="outfmt" type="string" string="- %msg% %$.lkp%\n")

set $.lkp = lookup("xlate", $msg);

if ($msg == " msgnum:00000002:") then {
  reload_lookup_table("xlate", "reload_failed");
}

action(type="omfile" file="./rsyslog.out.log" template="outfmt")
'
cp -f $srcdir/testsuites/xlate.lkp_tbl xlate.lkp_tbl
startup
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
shutdown_when_empty
echo wait on shutdown
wait_shutdown
. $srcdir/diag.sh content-check "msgnum:00000000: foo_latest"
. $srcdir/diag.sh content-check "msgnum:00000001: quux"
. $srcdir/diag.sh content-check "msgnum:00000002: baz_latest"
. $srcdir/diag.sh content-check "msgnum:00000000: reload_failed"
. $srcdir/diag.sh content-check "msgnum:00000000: reload_failed"

exit_test
