#!/bin/bash
# Native-YAML parity for transactional imtcp delimiter-regex replacement. The
# established socket must retain its old compiled regex and a later socket must
# use the new one. Second frame markers release the asserted records, providing
# a deterministic parser-state oracle without sleeps or reconnect assumptions.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp
generate_conf --yaml-only
sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "on"' "${TESTCONF_NM}.yaml"
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    address: "127.0.0.1"'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"'
add_yaml_conf '    name: regex'
add_yaml_conf '    ruleset: main'
add_yaml_conf '    framing.delimiter.regex: "^<33>Mar"'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    actions:'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        name: regex_sink'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '%s\n' \
	'<33>Mar  1 01:00:00 host app: yaml-regex-old-before' \
	'<33>Mar  1 01:00:01 host app: yaml-regex-old-before-flush' >&9 || error_exit 1
wait_content 'yaml-regex-old-before' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.base"

sed 's/\^<33>Mar/\^<34>Apr/' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=new_sessions"* ]]; then
	echo "FAIL: YAML delimiter regex did not activate for new sessions: $reload_status"
	error_exit 1
fi

printf '%s\n' \
	'<33>Mar  1 01:00:02 host app: yaml-regex-old-after' \
	'<33>Mar  1 01:00:03 host app: yaml-regex-old-after-flush' >&9 || error_exit 1
wait_content 'yaml-regex-old-after' "$RSYSLOG_OUT_LOG"

exec 8<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '%s\n' \
	'<34>Apr  1 01:00:00 host app: yaml-regex-new-after' \
	'<34>Apr  1 01:00:01 host app: yaml-regex-new-after-flush' >&8 || error_exit 1
wait_content 'yaml-regex-new-after' "$RSYSLOG_OUT_LOG"

# Private compilation must reject an invalid next pattern without publishing
# it or disturbing either already accepted session generation.
cp "$CONF_FILE" "$CONF_FILE.active"
sed 's/\^<34>Apr/[/' "$CONF_FILE.active" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activation_failed active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=new_sessions"* ]]; then
	echo "FAIL: invalid YAML regex did not roll back Prepare: $reload_status"
	error_exit 1
fi
printf '%s\n' \
	'<33>Mar  1 01:00:04 host app: yaml-regex-old-after-rollback' \
	'<33>Mar  1 01:00:05 host app: yaml-regex-old-after-rollback-flush' >&9 || error_exit 1
printf '%s\n' \
	'<34>Apr  1 01:00:02 host app: yaml-regex-new-after-rollback' \
	'<34>Apr  1 01:00:03 host app: yaml-regex-new-after-rollback-flush' >&8 || error_exit 1
wait_content 'yaml-regex-old-after-rollback' "$RSYSLOG_OUT_LOG"
wait_content 'yaml-regex-new-after-rollback' "$RSYSLOG_OUT_LOG"

exec 8>&-
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
