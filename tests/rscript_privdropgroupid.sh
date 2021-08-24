#!/bin/bash
# added 2021-09-23 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS"  "This test currently does not work on Solaris."
. $srcdir/privdrop_common.sh
rsyslog_testbench_setup_testuser

generate_conf
add_conf '
global(privdrop.group.keepsupplemental="on" privdrop.group.id="'${TESTBENCH_TESTUSER[gid]}'")
template(name="outfmt" type="list") {
	property(name="msg" compressSpace="on")
	constant(value="\n")
}
action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_when_empty
wait_shutdown
content_check --regex "groupid.*${TESTBENCH_TESTUSER[gid]}"
exit_test
