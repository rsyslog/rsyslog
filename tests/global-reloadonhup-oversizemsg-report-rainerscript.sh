#!/bin/bash
# Verify that RainerScript can transactionally publish oversizemsg.report.
# The imdiag test-only getter observes the active atomic policy before and after
# HUP. A post-commit marker proves that the daemon continues processing input.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(config.reloadOnHUP="on" oversizemsg.report="off")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup

report_mode="$(echo getreportoversizemsg | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$report_mode" != *": 0" ]]; then
	echo "FAIL: initial oversizemsg.report mode is not off: $report_mode"
	error_exit 1
fi

sed 's/oversizemsg.report="off"/oversizemsg.report="on"/' "$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: oversizemsg.report=on did not activate: $reload_status"
	error_exit 1
fi

report_mode="$(echo getreportoversizemsg | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$report_mode" != *": 1" ]]; then
	echo "FAIL: committed oversizemsg.report mode is not on: $report_mode"
	error_exit 1
fi
injectmsg_literal '<167>Mar 10 01:00:00 host app: oversize-report-live-marker'
wait_content 'oversize-report-live-marker' "$RSYSLOG_OUT_LOG"

# Two base-policy changes are outside the first live slice. Their rejection
# must not partially revert the already committed oversize-reporting value.
sed 's/oversizemsg.report="on"/oversizemsg.report="off" reportChildProcessExits="all"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: combined base-policy change was not rejected atomically: $reload_status"
	error_exit 1
fi
report_mode="$(echo getreportoversizemsg | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$report_mode" != *": 1" ]]; then
	echo "FAIL: rejected base-policy change altered oversizemsg.report: $report_mode"
	error_exit 1
fi

shutdown_when_empty
wait_shutdown
exit_test
