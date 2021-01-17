#!/bin/bash
# test for sparse-array lookup-table and HUP based reloading of it
# added 2015-10-30 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
lookup_table(name="xlate" file="'$RSYSLOG_DYNNAME'.xlate_array.lkp_tbl")

template(name="outfmt" type="string" string="%msg% %$.lkp%\n")

set $.num = field($msg, 58, 2);

set $.lkp = lookup("xlate", $.num);

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
cp -f $srcdir/testsuites/xlate_sparse_array.lkp_tbl $RSYSLOG_DYNNAME.xlate_array.lkp_tbl
startup
injectmsg  0 1
wait_queueempty
assert_content_missing "foo"
injectmsg  0 5
wait_queueempty
content_check "msgnum:00000001: foo_old"
content_check "msgnum:00000002: foo_old"
content_check "msgnum:00000003: bar_old"
content_check "msgnum:00000004: bar_old"
assert_content_missing "baz"
cp -f $srcdir/testsuites/xlate_sparse_array_more.lkp_tbl $RSYSLOG_DYNNAME.xlate_array.lkp_tbl
issue_HUP
await_lookup_table_reload
injectmsg  0 6
wait_queueempty
content_check "msgnum:00000000: foo_new"
content_check "msgnum:00000001: foo_new"
content_check "msgnum:00000002: bar_new"
content_check "msgnum:00000003: bar_new"
content_check "msgnum:00000004: baz"
content_check "msgnum:00000005: baz"
cp -f $srcdir/testsuites/xlate_sparse_array_more_with_duplicates_and_nomatch.lkp_tbl $RSYSLOG_DYNNAME.xlate_array.lkp_tbl
issue_HUP
await_lookup_table_reload
injectmsg  0 15
injectmsg  2147483647 3
injectmsg  2999999999 3
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
content_check "msgnum:00000000: quux"
content_check "msgnum:00000001: quux"
content_check "msgnum:00000002: foo_latest"
content_check "msgnum:00000003: baz_latest"
content_check "msgnum:00000004: foo_latest"
content_check "msgnum:00000005: foo_latest"
content_check "msgnum:00000006: foo_latest"
content_check "msgnum:00000007: foo_latest"
content_check "msgnum:00000008: baz_latest"
content_check "msgnum:00000009: baz_latest"
content_check "msgnum:00000010: baz_latest"
content_check "msgnum:00000011: baz_latest"
content_check "msgnum:00000012: foo_latest"
content_check "msgnum:00000013: foo_latest"
content_check "msgnum:00000014: foo_latest"
content_check "msgnum:2147483647: foo_latest"
content_check "msgnum:2147483648: gte_int_max"
content_check "msgnum:2147483649: gte_int_max"
content_check "msgnum:2999999999: gte_int_max"
content_check "msgnum:3000000000: gte_3B"
content_check "msgnum:3000000001: gte_3B"
exit_test
