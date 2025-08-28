#!/bin/bash
# add 2016-03-24 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS"  "This test currently does not work on Solaris."
. $srcdir/privdrop_common.sh
rsyslog_testbench_setup_testuser

generate_conf
add_conf '
template(name="outfmt" type="list") {
	property(name="msg" compressSpace="on")
	constant(value="\n")
}
action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
$PrivDropToUser '${TESTBENCH_TESTUSER[username]}'
'
startup
shutdown_when_empty
wait_shutdown
content_check --regex "userid.*${TESTBENCH_TESTUSER[uid]}"
exit_test
