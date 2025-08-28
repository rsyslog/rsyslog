#!/bin/bash
# add 2016-03-24 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
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
$PrivDropToGroupID '${TESTBENCH_TESTUSER[gid]}'
'
#add_conf "\$PrivDropToGroupID ${TESTBENCH_TESTUSER[gid]}"
startup
shutdown_when_empty
wait_shutdown
content_check --regex "groupid.*${TESTBENCH_TESTUSER[gid]}"
exit_test
