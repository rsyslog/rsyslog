#!/bin/bash
# test for lookup-table and HUP based reloading of it
# added 2015-09-30 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
lookup_table(name="xlate" file="'$RSYSLOG_DYNNAME'.xlate.lkp_tbl" reloadOnHUP="on")

template(name="outfmt" type="string" string="- %msg% %$.lkp%\n")

set $.lkp = lookup("xlate", $msg);

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
cp -f $srcdir/testsuites/xlate.lkp_tbl $RSYSLOG_DYNNAME.xlate.lkp_tbl
startup
injectmsg  0 3
wait_queueempty
content_check "msgnum:00000000: foo_old"
content_check "msgnum:00000001: bar_old"
assert_content_missing "baz"
cp -f $srcdir/testsuites/xlate_more.lkp_tbl $RSYSLOG_DYNNAME.xlate.lkp_tbl
issue_HUP
await_lookup_table_reload
injectmsg  0 3
wait_queueempty
content_check "msgnum:00000000: foo_new"
content_check "msgnum:00000001: bar_new"
content_check "msgnum:00000002: baz"
cp -f $srcdir/testsuites/xlate_more_with_duplicates_and_nomatch.lkp_tbl $RSYSLOG_DYNNAME.xlate.lkp_tbl
issue_HUP
await_lookup_table_reload
injectmsg  0 10
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
content_check "msgnum:00000000: foo_latest"
content_check "msgnum:00000001: quux"
content_check "msgnum:00000002: baz_latest"
content_check "msgnum:00000003: foo_latest"
content_check "msgnum:00000004: foo_latest"
content_check "msgnum:00000005: baz_latest"
content_check "msgnum:00000006: foo_latest"
content_check "msgnum:00000007: baz_latest"
content_check "msgnum:00000008: baz_latest"
content_check "msgnum:00000009: quux"
exit_test
