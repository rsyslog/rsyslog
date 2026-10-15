#!/bin/bash
# Verify native-YAML parity for transactional reportChildProcessExits updates.
# The one-record helper makes each successful child exit observable on the next
# direct omprog action. A persistent imtcp session feeds the named YAML ruleset;
# a constant RainerScript include routes internal diagnostics to the oracle
# file. The changed global policy and executable ruleset remain native YAML.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin omprog
require_plugin imtcp

helper="$RSYSLOG_DYNNAME.reportchild-helper.sh"
cat >"$helper" <<'HELPER_EOF'
#!/bin/sh
printf 'OK\n'
if IFS= read -r line; then
	printf 'OK\n'
fi
HELPER_EOF
chmod +x "$helper"

generate_conf --yaml-only
yaml_conf="${TESTCONF_NM}.yaml"
sed -i '/debug.abortOnProgramError:/a\  processInternalMessages: "on"\
  config.reloadOnHUP: "on"\
  reportChildProcessExits: "errors"' "$yaml_conf"
printf 'action(type="omfile" file="%s")\n' "$RSYSLOG_OUT_LOG" >"$RSYSLOG_DYNNAME.internal.conf"
cat >>"$yaml_conf" <<YAML_EOF
include:
  - path: "$RSYSLOG_DYNNAME.internal.conf"
modules:
  - load: "../plugins/omprog/.libs/omprog"
  - load: "../plugins/imtcp/.libs/imtcp"
templates:
  - name: reportchild_fmt
    type: string
    string: "%msg%\\n"
inputs:
  - type: imtcp
    port: "0"
    listenPortFileName: "$RSYSLOG_DYNNAME.tcpflood_port"
    ruleset: main
rulesets:
  - name: main
    statements:
      - if: '\$msg contains "reportchild-"'
        then:
          - type: omprog
            name: reportchild_program
            binary: "$(pwd)/$helper"
            template: reportchild_fmt
            confirmMessages: "on"
            signalOnClose: "off"
            queue.type: Direct
      - type: omfile
        name: reportchild_sink
        file: "$RSYSLOG_OUT_LOG"
YAML_EOF
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"

printf '<167>Mar 10 01:00:00 host app: reportchild-errors-0\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: reportchild-errors-1\n' >&9 || error_exit 1
wait_content 'reportchild-errors-1' "$RSYSLOG_OUT_LOG"
wait_queueempty
custom_assert_content_missing 'exited with status 0' "$RSYSLOG_OUT_LOG"

sed 's/reportChildProcessExits: "errors"/reportChildProcessExits: "all"/' "$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: YAML reportChildProcessExits=all did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: reportchild-all-0\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: reportchild-all-1\n' >&9 || error_exit 1
wait_content 'reportchild-all-1' "$RSYSLOG_OUT_LOG"
wait_content 'exited with status 0' "$RSYSLOG_OUT_LOG"
report_count=$(awk '/exited with status 0/ { count++ } END { print count + 0 }' "$RSYSLOG_OUT_LOG")

sed 's/reportChildProcessExits: "all"/reportChildProcessExits: "none"/' "$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: YAML reportChildProcessExits=none did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: reportchild-none-0\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: reportchild-none-1\n' >&9 || error_exit 1
wait_content 'reportchild-none-1' "$RSYSLOG_OUT_LOG"
wait_queueempty
final_report_count=$(awk '/exited with status 0/ { count++ } END { print count + 0 }' "$RSYSLOG_OUT_LOG")
if [[ "$final_report_count" -ne "$report_count" ]]; then
	echo "FAIL: YAML reportChildProcessExits=none emitted a new successful-child diagnostic"
	error_exit 1
fi

shutdown_when_empty
wait_shutdown
exit_test
