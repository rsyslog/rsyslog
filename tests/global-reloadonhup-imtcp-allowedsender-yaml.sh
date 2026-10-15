#!/bin/bash
# Native-YAML parity for fenced allowedSender reload.  The policy is widened
# while the established session remains permitted, then narrowed while a
# partial non-LF frame is buffered.  The partial frame must not be flushed by
# revocation, and a new connection after reallow proves synchronous
# maxSessions-slot and persistent-descriptor reclamation.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp
require_plugin impstats
export STATSFILE="$RSYSLOG_DYNNAME.stats"
generate_conf --yaml-only
sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "on"' "${TESTCONF_NM}.yaml"
sed -i '/config.reloadOnHUP:/a\  net.aclResolveHostname: "off"' "${TESTCONF_NM}.yaml"
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf '    maxSessions: 1'
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

# A changed policy that still permits localhost must retain the stream.
sed 's/allowedSender: \["127\.0\.0\.1\/32"\]/allowedSender: ["127.0.0.1\/32", "192.0.2.1\/32"]/' \
	"$CONF_FILE.allowed" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: YAML ACL expansion did not activate live: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: acl-yaml-survives\n' >&9 || error_exit 1
wait_content 'acl-yaml-survives' "$RSYSLOG_OUT_LOG"

# Ensure the revoked session has consumed a partial frame before the fence.
# The imtcp byte counter is cumulative; waiting for it to increase after the
# write is the state oracle that the non-LF bytes reached imtcp without being
# submitted as a message.
wait_content 'origin=imtcp.*bytes.received=' "$STATSFILE"
partial_bytes_before=$(awk -F'bytes.received=' '/origin=imtcp/ && /bytes.received=/ { split($2, v, /[^0-9]/); value=v[1] } END { print value + 0 }' "$STATSFILE")
wait_partial_bytes() {
	local current
	if [ ! -f "$STATSFILE" ]; then
		echo 0
		return
	fi
	current=$(awk -F'bytes.received=' '/origin=imtcp/ && /bytes.received=/ { split($2, v, /[^0-9]/); value=v[1] } END { print value + 0 }' "$STATSFILE")
	if [ "$current" -gt "$partial_bytes_before" ]; then
		echo 1
	else
		echo 0
	fi
}
printf '<167>Mar 10 01:00:00 host app: acl-yaml-partial' >&9 || error_exit 1
wait_file_lines --count-function wait_partial_bytes "$STATSFILE" 1

sed 's/allowedSender: \["127\.0\.0\.1\/32", "192\.0\.2\.1\/32"\]/allowedSender: ["192.0.2.1\/32"]/' \
	"$CONF_FILE" >"$CONF_FILE.revoked"
cp "$CONF_FILE.revoked" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: YAML ACL revocation did not activate live: $reload_status"
	error_exit 1
fi
assert_content_missing 'acl-yaml-partial'

# Reallow and connect again.  This succeeds only when revocation reclaimed the
# sole maxSessions slot and persistent descriptor before fence release.
cp "$CONF_FILE.allowed" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=4"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: YAML ACL reallow did not activate live: $reload_status"
	error_exit 1
fi
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: acl-yaml-after\n' >&9 || error_exit 1
wait_content 'acl-yaml-after' "$RSYSLOG_OUT_LOG"
exec 9>&-
shutdown_when_empty
wait_shutdown
# Drain the action queue before the final negative oracle, so a buggy regular
# close cannot hide a flushed partial frame behind asynchronous output.
assert_content_missing 'acl-yaml-partial'
exit_test
