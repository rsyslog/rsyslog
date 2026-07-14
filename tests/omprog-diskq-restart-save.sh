#!/bin/bash
# Regression coverage for GitHub issue #5364. An omprog action with a pure disk
# action queue receives a negative acknowledgement, then rsyslog is restarted
# before the action can drain. The oracle is that the message is still retried
# after restart and is acknowledged once the helper switches to success mode.
# QUEUE_TYPE lets the segmentedDisk wrapper run the identical retry oracle.

. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=1
QUEUE_TYPE=${QUEUE_TYPE:-Disk}
export RSYSLOG_OMPROG_RESTART_OK="$PWD/$RSYSLOG_DYNNAME.allow-ok"
export RSYSLOG_OMPROG_RESTART_OUT="$PWD/$RSYSLOG_OUT_LOG"

cp -f "$srcdir/testsuites/omprog-diskq-restart-save-bin.sh" \
	"$RSYSLOG_DYNNAME.omprog-diskq-restart-save-bin.sh"
chmod +x "$RSYSLOG_DYNNAME.omprog-diskq-restart-save-bin.sh"

generate_conf
add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME'.spool")
module(load="../plugins/omprog/.libs/omprog")

template(name="outfmt" type="string" string="%msg%\n")

if $msg contains "msgnum:" then {
	action(
		type="omprog"
		binary="'$RSYSLOG_DYNNAME'.omprog-diskq-restart-save-bin.sh"
		template="outfmt"
		name="omprog_action"
		confirmMessages="on"
		confirmTimeout="1000"
		reportFailures="on"
		action.resumeRetryCount="-1"
		action.resumeInterval="1"
		queue.type="'$QUEUE_TYPE'"
		queue.filename="'$RSYSLOG_DYNNAME'.actq"
		queue.syncqueuefiles="on"
		queue.checkpointInterval="1"
		queue.workerThreads="1"
		queue.timeoutShutdown="1000"
		queue.timeoutActionCompletion="1000"
		queue.saveOnShutdown="on"
	)
}
'

startup
injectmsg
for _ in {1..30}; do
	if content_check --check-only "<= ERROR"; then
		break
	fi
	$TESTTOOL_DIR/msleep 1000
done
content_check "<= ERROR"

shutdown_immediate
wait_shutdown

touch "$RSYSLOG_OMPROG_RESTART_OK"
startup
for _ in {1..90}; do
	ok_count=0
	if [ -f "$RSYSLOG_OMPROG_RESTART_OUT" ]; then
		ok_count=$(grep -c -F -- "<= OK" "$RSYSLOG_OMPROG_RESTART_OUT")
	fi
	if [ "$ok_count" -ge 3 ]; then
		break
	fi
	$TESTTOOL_DIR/msleep 1000
done
shutdown_when_empty
wait_shutdown

content_check "=>  msgnum:00000000:"
content_check "<= ERROR"
content_count_check "<= OK" 3

exit_test
