#!/bin/bash
# Regression coverage for issue #4803. A slow omhttp action with a
# disk-assisted action queue is terminated while work is still in flight. The
# oracle is process-level shutdown without an assertion/core; the test
# intentionally unsets the testbench timeout-to-stderr knob because that knob
# suppresses the debug-build worker-destruction assertion reported by the issue.

. ${srcdir:=.}/diag.sh init

unset RSYSLOG_DEBUG_TIMEOUTS_TO_STDERR
export NUMMESSAGES=500
export RS_REDIR=">$RSYSLOG_DYNNAME.rsyslog.log 2>&1"

omhttp_start_server 0 --fail-every 2 --fail-with-delay-secs 10
# omhttp_start_server assigns this helper variable after the listener binds.
# shellcheck disable=SC2154
port="$omhttp_server_lstnport"

generate_conf
add_conf '
module(load="../contrib/omhttp/.libs/omhttp")

global(workDirectory="'$RSYSLOG_DYNNAME'.spool")

template(name="tpl" type="string" string="%msg%\n")

if $msg contains "msgnum:" then {
	action(
		name="action_omhttp_shutdown"
		type="omhttp"
		server="localhost"
		serverport="'$port'"
		restpath="my/endpoint"
		usehttps="off"
		restpathtimeout="30000"
		template="tpl"
		batch="off"
		action.resumeRetryCount="-1"
		action.resumeInterval="1"
		queue.type="LinkedList"
		queue.filename="'$RSYSLOG_DYNNAME'.actq"
		queue.saveonshutdown="on"
		queue.timeoutshutdown="1"
		queue.timeoutactioncompletion="1"
	)
}
'

startup
injectmsg
shutdown_immediate
wait_shutdown "" 60
check_not_present "Assertion" "$RSYSLOG_DYNNAME.rsyslog.log"
check_not_present "worker not stopped during shutdown" "$RSYSLOG_DYNNAME.rsyslog.log"
omhttp_stop_server
exit_test
