#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify native-YAML parity for transactional oversizemsg.report publication.
# Native globals fix a 512-byte truncate limit; generated 1024-byte submissions
# prove warning suppression before HUP and configured warning output afterward.
# A constant RainerScript include supplies only the unchanged action.
. ${srcdir:=.}/diag.sh init
require_yaml_support

generate_conf --yaml-only
yaml_conf="${TESTCONF_NM}.yaml"
sed -i '/debug.abortOnProgramError:/a\  processInternalMessages: "on"\
  config.reloadOnHUP: "on"\
  oversizemsg.report: "off"\
  maxMessageSize: "512"\
  oversizemsg.input.mode: "truncate"' "$yaml_conf"
cat >"$RSYSLOG_DYNNAME.internal.conf" <<RS_EOF
action(type="omfile" file="$RSYSLOG_OUT_LOG")
RS_EOF
cat >>"$yaml_conf" <<YAML_EOF
include:
  - path: "$RSYSLOG_DYNNAME.internal.conf"
YAML_EOF
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
	echo "FAIL: initial YAML oversizemsg.report mode is not off: $report_mode"
	error_exit 1
fi

sed 's/oversizemsg.report: "off"/oversizemsg.report: "on"/' "$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: YAML oversizemsg.report=on did not activate: $reload_status"
	error_exit 1
fi

report_mode="$(echo getreportoversizemsg | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$report_mode" != *": 1" ]]; then
	echo "FAIL: committed YAML oversizemsg.report mode is not on: $report_mode"
	error_exit 1
fi
injectmsg_literal '<167>Mar 10 01:00:00 host app: yaml-oversize-report-live-marker'
wait_content 'yaml-oversize-report-live-marker' "$RSYSLOG_OUT_LOG"
inject_oversized oversize-after
wait_content 'message too long.*oversize-after' "$RSYSLOG_OUT_LOG"

# A second base-policy change must make the entire candidate unsupported; the
# previously activated oversize-reporting value remains visible afterward.
sed -e 's/oversizemsg.report: "on"/oversizemsg.report: "off"/' \
	-e '/oversizemsg.report: "off"/a\  reportChildProcessExits: "all"' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: combined YAML base-policy change was not rejected atomically: $reload_status"
	error_exit 1
fi
report_mode="$(echo getreportoversizemsg | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$report_mode" != *": 1" ]]; then
	echo "FAIL: rejected YAML base-policy change altered oversizemsg.report: $report_mode"
	error_exit 1
fi

inject_oversized oversize-after-reject
wait_content 'message too long.*oversize-after-reject' "$RSYSLOG_OUT_LOG"
shutdown_when_empty
wait_shutdown
content_check --regex 'message too long.*oversize-after' "$RSYSLOG_OUT_LOG"
exit_test
