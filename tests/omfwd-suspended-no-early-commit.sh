#!/bin/bash
# Regression coverage for GitHub issue #5132. A transactional action that has
# exhausted its immediate resume attempts must not call the module commit hook
# again while its next retry timestamp is still in the future. The oracle is
# the action/omfwd debug log: we wait until actionTryCommit reports that it
# skipped transaction processing for the suspended action, then assert that no
# commitTransaction call entered while the action state was still "susp".
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debuglog"

generate_conf
add_conf '
global(
	debug.whitelist="on"
	debug.files=["action.c", "../action.c", "../../runtime/action.c", "../../../runtime/action.c", "omfwd.c"]
)

template(name="outfmt" type="string" string="%msg%\n")

:msg, contains, "msgnum:" {
	action(
		name="forwarder"
		type="omfwd"
		target="127.0.0.1"
		port="'$TCPFLOOD_PORT'"
		protocol="tcp"
		template="outfmt"
		queue.type="LinkedList"
		queue.dequeueBatchSize="1"
		action.resumeInterval="1"
		action.resumeIntervalMax="2"
		action.resumeRetryCount="0"
	)
}
'
startup
injectmsg 0 1

wait_content "actionTryCommit\\[forwarder\\]: skip transaction while state is susp" "$RSYSLOG_DEBUGLOG"
shutdown_immediate
wait_shutdown

check_not_present "entering actionCallCommitTransaction[forwarder], state: susp" "$RSYSLOG_DEBUGLOG"
exit_test
