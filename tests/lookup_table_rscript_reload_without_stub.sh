#!/bin/bash
# test for lookup-table reload by rscript-stmt without stub
# added 2015-12-18 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "FreeBSD"  "This test currently does not work on FreeBSD"
generate_conf
add_conf '
lookup_table(name="xlate" file="'$RSYSLOG_DYNNAME'.xlate.lkp_tbl")

template(name="outfmt" type="string" string="- %msg% %$.lkp%\n")

set $.lkp = lookup("xlate", $msg);

if ($msg == " msgnum:00000002:") then {
  reload_lookup_table("xlate")
}

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
cp -f $srcdir/testsuites/xlate.lkp_tbl $RSYSLOG_DYNNAME.xlate.lkp_tbl
startup
# the last message ..002 should cause successful lookup-table reload
cp -f $srcdir/testsuites/xlate_more.lkp_tbl $RSYSLOG_DYNNAME.xlate.lkp_tbl
injectmsg  0 3
await_lookup_table_reload
wait_queueempty
content_check "msgnum:00000000: foo_old"
content_check "msgnum:00000001: bar_old"
assert_content_missing "baz"
cp -f $srcdir/testsuites/xlate_more_with_duplicates_and_nomatch.lkp_tbl $RSYSLOG_DYNNAME.xlate.lkp_tbl
injectmsg  0 3
await_lookup_table_reload
wait_queueempty
content_check "msgnum:00000000: foo_new"
content_check "msgnum:00000001: bar_new"
content_check "msgnum:00000002: baz"
rm -f $RSYSLOG_DYNNAME.xlate.lkp_tbl # this should lead to unsuccessful reload
injectmsg  0 3
await_lookup_table_reload
wait_queueempty
injectmsg  0 2
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
content_check_with_count "msgnum:00000000: foo_latest" 2
content_check_with_count "msgnum:00000001: quux" 2
content_check_with_count "msgnum:00000002: baz_latest" 1

exit_test
