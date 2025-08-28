#!/bin/bash
# add 2016-03-24 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS"  "This test currently does not work on Solaris."
. $srcdir/privdrop_common.sh
rsyslog_testbench_setup_testuser

generate_conf
add_conf '
global(privdrop.group.keepsupplemental="on")
template(name="outfmt" type="list") {
	property(name="msg" compressSpace="on")
	constant(value="\n")
}
action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
$PrivDropToGroup '${TESTBENCH_TESTUSER[groupname]}'
'
startup
shutdown_when_empty
wait_shutdown
content_check --regex "groupid.*${TESTBENCH_TESTUSER[gid]}"
if [ ! $? -eq 0 ]; then
  echo "message indicating drop to group \"${TESTBENCH_TESTUSER[groupname]}\" (#${TESTBENCH_TESTUSER[gid]}) is missing:"
  cat $RSYSLOG_OUT_LOG
  error_exit 1
fi;

exit_test
