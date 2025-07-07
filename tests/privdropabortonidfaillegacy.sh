#!/bin/bash
# add 2021-10-12 by alorbach, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debuglog"

skip_platform "SunOS"  "This test currently does not work on Solaris."
export TESTBENCH_TESTUSER1="USER_${RSYSLOG_DYNNAME}_1"
export TESTBENCH_TESTUSER2="USER_${RSYSLOG_DYNNAME}_2"

generate_conf
add_conf '
global(
	security.abortOnIDResolutionFail="off"
)

template(name="outfmt" type="list") {
	property(name="msg" compressSpace="on")
	constant(value="\n")
}

$FileOwner '${TESTBENCH_TESTUSER1}'
$FileGroup '${TESTBENCH_TESTUSER1}'
$DirOwner '${TESTBENCH_TESTUSER2}'
$DirGroup '${TESTBENCH_TESTUSER2}'

action(	type="omfile"
	template="outfmt"
	file=`echo $RSYSLOG_OUT_LOG`)
'

startup
shutdown_when_empty
wait_shutdown
content_check --regex "ID for user '${TESTBENCH_TESTUSER1}' could not be found"
content_check --regex "ID for user '${TESTBENCH_TESTUSER2}' could not be found"

exit_test
