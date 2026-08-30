#!/bin/bash
# Verify the native YAML frontend reaches the same live numeric allowedSender
# path.  impstats proves the established session was denied after the fence;
# successful delivery on that same descriptor after reallow proves retention.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp
require_plugin impstats
export STATSFILE="$RSYSLOG_DYNNAME.stats"
generate_conf --yaml-only
sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "on"' "${TESTCONF_NM}.yaml"
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf '    allowedSender: ["127.0.0.1/32"]'
add_yaml_conf '  - load: "../plugins/impstats/.libs/impstats"'
add_yaml_conf '    log.file: "'$STATSFILE'"'
add_yaml_conf '    interval: 1'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    name: acl-yaml'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"'
add_yaml_conf '    ruleset: main'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    statements:'
add_yaml_conf '      - if: '\''$msg contains "acl-yaml"'\'''
add_yaml_conf '        action:'
add_yaml_conf '          type: omfile'
add_yaml_conf '          file: "'$RSYSLOG_OUT_LOG'"'
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: acl-yaml-before\n' >&9 || error_exit 1
wait_content 'acl-yaml-before' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.allowed"

sed 's/127\.0\.0\.1\/32/192.0.2.1\/32/' "$CONF_FILE.allowed" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: numeric YAML ACL did not activate live: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: acl-yaml-denied\n' >&9 || error_exit 1
wait_content 'reload_acl_message_dropped_total=1' "$STATSFILE"
assert_content_missing 'acl-yaml-denied' "$RSYSLOG_OUT_LOG"

cp "$CONF_FILE.allowed" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: numeric YAML ACL reallow did not activate live: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: acl-yaml-after\n' >&9 || error_exit 1
wait_content 'acl-yaml-after' "$RSYSLOG_OUT_LOG"

# Input-local YAML values override the restored module ACL.  A second drop on
# the same descriptor proves effective module/input precedence during reload.
sed '/ruleset: main/a\    allowedSender: ["192.0.2.1/32"]' "$CONF_FILE.allowed" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=4"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: YAML input ACL override did not activate live: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: acl-yaml-input-denied\n' >&9 || error_exit 1
wait_content 'reload_acl_message_dropped_total=2' "$STATSFILE"
assert_content_missing 'acl-yaml-input-denied' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE.allowed" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=5"* ]]; then
	echo "FAIL: YAML input ACL override was not removed live: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: acl-yaml-final\n' >&9 || error_exit 1
wait_content 'acl-yaml-final' "$RSYSLOG_OUT_LOG"

# A changed hostname ACL remains restart-required because live preparation may
# not perform DNS.  Rejection must preserve generation 5 and the restored
# numeric policy on this same connection.
sed 's/127\.0\.0\.1\/32/\*.example.invalid/' "$CONF_FILE.allowed" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=5"* ||
      "$reload_status" != *"source_capability=restart_required"* ]]; then
	echo "FAIL: YAML hostname ACL did not reject atomically: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: acl-yaml-after-reject\n' >&9 || error_exit 1
wait_content 'acl-yaml-after-reject' "$RSYSLOG_OUT_LOG"
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
