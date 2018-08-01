#!/bin/bash
# added 2015-12-02 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[lookup_table_bad_configs.sh\]: test for sparse-array lookup-table and HUP based reloading of it
. $srcdir/diag.sh init
generate_conf
add_conf '
lookup_table(name="xlate" file="xlate.lkp_tbl")

template(name="outfmt" type="string" string="%msg% %$.lkp%\n")

set $.num = field($msg, 58, 2);

set $.lkp = lookup("xlate", $.num);

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'

echo "empty file..."
cp -f $srcdir/testsuites/xlate_empty_file.lkp_tbl xlate.lkp_tbl
startup_vg
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh wait-queueempty

echo "table with invalid-json..."
cp -f $srcdir/testsuites/xlate_invalid_json.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh wait-queueempty

echo "string-table with no index-key..."
cp -f $srcdir/testsuites/xlate_string_no_index.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh assert-content-missing "foo"
. $srcdir/diag.sh assert-content-missing "bar"
. $srcdir/diag.sh assert-content-missing "baz"

echo "array-table with no index-key..."
cp -f $srcdir/testsuites/xlate_array_no_index.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh assert-content-missing "foo"
. $srcdir/diag.sh assert-content-missing "bar"
. $srcdir/diag.sh assert-content-missing "baz"

echo "sparse-array-table with no index-key..."
cp -f $srcdir/testsuites/xlate_sparseArray_no_index.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh assert-content-missing "foo"
. $srcdir/diag.sh assert-content-missing "bar"
. $srcdir/diag.sh assert-content-missing "baz"

echo "string-table with no value..."
cp -f $srcdir/testsuites/xlate_string_no_value.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh assert-content-missing "baz"

echo "array-table with no value..."
cp -f $srcdir/testsuites/xlate_array_no_value.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh assert-content-missing "baz"

echo "sparse-array-table with no value..."
cp -f $srcdir/testsuites/xlate_sparseArray_no_value.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh assert-content-missing "baz"

echo "incorrect-version in lookup-table..."
cp -f $srcdir/testsuites/xlate_incorrect_version.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh assert-content-missing "foo"
. $srcdir/diag.sh assert-content-missing "bar"
. $srcdir/diag.sh assert-content-missing "baz"

echo "incorrect-type in lookup-table..."
cp -f $srcdir/testsuites/xlate_incorrect_type.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh assert-content-missing "foo"
. $srcdir/diag.sh assert-content-missing "bar"
. $srcdir/diag.sh assert-content-missing "baz"

echo "string-table with no table..."
cp -f $srcdir/testsuites/xlate_string_no_table.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh assert-content-missing "baz"

echo "array-table with no table..."
cp -f $srcdir/testsuites/xlate_array_no_table.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh assert-content-missing "baz"

echo "sparse-array-table with no table..."
cp -f $srcdir/testsuites/xlate_sparseArray_no_table.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh assert-content-missing "baz"

echo "string-table with empty table..."
cp -f $srcdir/testsuites/xlate_string_empty_table.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 2
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000000: baz_str"
. $srcdir/diag.sh content-check "msgnum:00000001: baz_str"

echo "array-table with empty table..."
cp -f $srcdir/testsuites/xlate_array_empty_table.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 2
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000000: baz_arr"
. $srcdir/diag.sh content-check "msgnum:00000001: baz_arr"

echo "sparse-array-table with empty table..."
cp -f $srcdir/testsuites/xlate_sparseArray_empty_table.lkp_tbl xlate.lkp_tbl
. $srcdir/diag.sh issue-HUP
. $srcdir/diag.sh await-lookup-table-reload
. $srcdir/diag.sh injectmsg  0 2
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000000: baz_sparse_arr"
. $srcdir/diag.sh content-check "msgnum:00000001: baz_sparse_arr"

echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
exit_test
