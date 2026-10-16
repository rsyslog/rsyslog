#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify that RainerScript can transactionally publish oversizemsg.report.
# The getter and real oversized submissions observe the policy across HUP.
# A generated 1024-byte payload exceeds the fixed 512-byte limit: its retained marker
# proves processing, while configured output proves warning suppression/on.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(config.reloadOnHUP="on" oversizemsg.report="off"
       processInternalMessages="on" maxMessageSize="512" oversizemsg.input.mode="truncate")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
# Generate after diagnostic command framing; a long literal command would
# itself be truncated at 512 bytes. The acknowledgement proves submission.
inject_oversized() {
	local reply
	reply="$(echo "injectmsg oversized $1" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
	[[ "$reply" == *"1 messages injected"* ]] || error_exit 1
}
inject_oversized oversize-before
wait_content 'oversize-before' "$RSYSLOG_OUT_LOG"
wait_queueempty
custom_assert_content_missing 'message too long' "$RSYSLOG_OUT_LOG"

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
inject_oversized oversize-after
wait_content 'message too long.*oversize-after' "$RSYSLOG_OUT_LOG"

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

inject_oversized oversize-after-reject
wait_content 'message too long.*oversize-after-reject' "$RSYSLOG_OUT_LOG"
shutdown_when_empty
wait_shutdown
content_check --regex 'message too long.*oversize-after' "$RSYSLOG_OUT_LOG"
exit_test
