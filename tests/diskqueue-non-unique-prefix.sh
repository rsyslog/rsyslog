#!/bin/bash
# Test for config parser to check that disk queue file names are
# unique.
# added 2019-05-02 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
if 0 then {
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.notused"
		queue.filename="qf1" queue.type="linkedList")
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.notused"
		queue.filename="qf1" queue.type="linkedList")
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.notused"
		queue.filename="qf2" queue.type="linkedList")
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.notused"
		queue.spooldirectory="'$RSYSLOG_DYNNAME'.spool2"
		queue.filename="qf2" queue.type="linkedList")
}

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_when_empty
wait_shutdown
content_check "and file name prefix 'qf1' already used"
check_not_present "and file name prefix 'qf2' already used"
exit_test
