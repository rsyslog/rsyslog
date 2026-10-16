#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Native-YAML parity for fenced allowedSender reload.  The policy is widened
# while the established session remains permitted, then narrowed while a
# partial non-LF frame is buffered.  The partial frame must not be flushed by
# revocation, and a new connection after reallow proves synchronous
# maxSessions-slot and persistent-descriptor reclamation.
# Also cover wildcard/unresolved policy, occupied-tail shrink+deny rollback,
# input override and legacy fallback. Rejection wrappers use DNS defaults.
# Bounded reads require EOF with no data, not timeout; .started lifecycle
# notices acknowledge that the truncated tail is free before retry.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp
require_plugin impstats
export STATSFILE="$RSYSLOG_DYNNAME.stats"
generate_conf --yaml-only
# A constant include captures daemon lifecycle diagnostics only; all input
# policy and every candidate remain native YAML, as in capacity coverage.
printf 'action(type="omfile" file="%s")\n' "$RSYSLOG_DYNNAME.started" >"$RSYSLOG_DYNNAME.internal.conf"
add_yaml_conf 'include:'
add_yaml_conf '  - path: "'$RSYSLOG_DYNNAME'.internal.conf"'
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
add_yaml_conf '    notifyOnConnectionClose: "on"'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"'
add_yaml_conf '    ruleset: main'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    name: acl-control'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.control-port"'
add_yaml_conf '    allowedSender: ["127.0.0.1/32"]'
add_yaml_conf '    ruleset: main'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    statements:'
add_yaml_conf '      - if: '\''$msg contains "acl-yaml"'\'''
add_yaml_conf '        action:'
add_yaml_conf '          type: omfile'
add_yaml_conf '          file: "'$RSYSLOG_OUT_LOG'"'
if [[ -n "${RSYSLOG_ACL_RELOAD_REJECTIONS:-}" ]]; then
	sed -i '/net.aclResolveHostname:/d' "${TESTCONF_NM}.yaml"
fi
startup
wait_file_exists "$RSYSLOG_DYNNAME.control-port"
control_port=$(cat "$RSYSLOG_DYNNAME.control-port")
exec 8<>"/dev/tcp/127.0.0.1/$control_port"
control_sequence=0
assert_control() {
	control_sequence=$((control_sequence + 1))
	printf '<167>Mar 10 01:00:00 host app: acl-yaml-control-%s\n' "$control_sequence" >&8 || error_exit 1
	wait_content "acl-yaml-control-$control_sequence" "$RSYSLOG_OUT_LOG"
}
assert_control
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: acl-yaml-before\n' >&9 || error_exit 1
wait_content 'acl-yaml-before' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.allowed"

if [[ -n "${RSYSLOG_ACL_RELOAD_REJECTIONS:-}" ]]; then
	# DNS-resolved bare-host changes cannot be prepared safely. The existing
	# stream and generation must survive, not merely a successful new handshake.
	sed '0,/127[.]0[.]0[.]1\/32/s@127[.]0[.]0[.]1/32@localhost@' "$CONF_FILE.allowed" >"$CONF_FILE"
	issue_HUP
	reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
	[[ "$reload_status" == *"result=candidate_scope_unsupported active_generation=1"* &&
	   "$reload_status" == *"source_capability=restart_required"* ]] || error_exit 1
	rejection=0
	for policy in '[]' '["127.0.0.1/32 extra"]' legacy; do
		rejection=$((rejection + 1))
		if [[ "$policy" == legacy ]]; then
			cp "$CONF_FILE.allowed" "$CONF_FILE"
			printf '$AllowedSender TCP, 192.0.2.1\n' >"$CONF_FILE.legacy.conf"
			printf '\ninclude:\n  - path: "%s"\n' "$CONF_FILE.legacy.conf" >>"$CONF_FILE"
		else
			sed '0,/\["127[.]0[.]0[.]1\/32"\]/s@\["127[.]0[.]0[.]1/32"\]@'"$policy"'@' "$CONF_FILE.allowed" >"$CONF_FILE"
		fi
		issue_HUP
		reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
		# Syntax rejection is frontend-specific for an empty array. Both
		# frontends reject legacy directives during capture, before capability.
		expected=candidate_normalization_unsupported
		[[ "$policy" == legacy ]] && expected=candidate_parse_invalid
		# Native YAML retains empty-array syntax for semantic validation.
		[[ "$reload_status" == *"result=$expected active_generation=1"* ]] ||
			error_exit 1 "$policy: $reload_status"
		printf '<167>Mar 10 01:00:00 host app: acl-yaml-rejection-%s\n' "$rejection" >&9 || error_exit 1
		wait_content "acl-yaml-rejection-$rejection" "$RSYSLOG_OUT_LOG"
		assert_control
	done
	exec 8>&-
	exec 9>&-
	shutdown_when_empty
	wait_shutdown
	exit_test
fi

# A changed policy that still permits localhost must retain the stream.
sed '0,/allowedSender/s@allowedSender: \["127\.0\.0\.1/32"\]@allowedSender: ["127.0.0.1/32", "192.0.2.1/32"]@' \
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
wait_content 'acl-yaml.*origin=imtcp.*bytes.received=' "$STATSFILE"
partial_bytes_before=$(awk -F'bytes.received=' '/acl-yaml/ && /origin=imtcp/ && /bytes.received=/ { split($2, v, /[^0-9]/); value=v[1] } END { print value + 0 }' "$STATSFILE")
wait_partial_bytes() {
	local current
	if [ ! -f "$STATSFILE" ]; then
		echo 0
		return
	fi
	current=$(awk -F'bytes.received=' '/acl-yaml/ && /origin=imtcp/ && /bytes.received=/ { split($2, v, /[^0-9]/); value=v[1] } END { print value + 0 }' "$STATSFILE")
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
assert_control

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
# Repeat with the replacement stream still open: both cycles must reclaim
# the sole slot and descriptor, not merely mark a denied session.
cp "$CONF_FILE.revoked" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
[[ "$reload_status" == *"result=activated active_generation=5"* ]] || error_exit 1
cp "$CONF_FILE.allowed" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
[[ "$reload_status" == *"result=activated active_generation=6"* ]] || error_exit 1
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: acl-yaml-repeat\n' >&9 || error_exit 1
wait_content 'acl-yaml-repeat' "$RSYSLOG_OUT_LOG"
# Textual policies must use the same close/reclaim contract without DNS work.
# EOF (not a timeout) proves revocation; reconnecting the single slot proves
# reclamation. The explicit input ACL control must survive every module change.
generation=6
activate_policy() {
	generation=$((generation + 1))
	issue_HUP
	reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
	[[ "$reload_status" == *"result=activated active_generation=$generation"* ]] || error_exit 1
	assert_control
}
for denied in '*.example.invalid' 'definitely-no-match.example.invalid'; do
	# Replace only the module entry; the control's input override is unchanged.
	sed '0,/127[.]0[.]0[.]1\/32/s@127[.]0[.]0[.]1/32@'"$denied"'@' "$CONF_FILE.allowed" >"$CONF_FILE"
	activate_policy
	IFS= read -r -t 10 -u 9 denied_frame
	closed=$?
	[[ "$closed" -eq 1 && -z "$denied_frame" ]] || error_exit 1
	sed 's@127\.0\.0\.1/32@*@' "$CONF_FILE.allowed" >"$CONF_FILE"
	activate_policy
	exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
	printf '<167>Mar 10 01:00:00 host app: acl-yaml-textual-%s\n' "$generation" >&9 || error_exit 1
	wait_content "acl-yaml-textual-$generation" "$RSYSLOG_OUT_LOG"
done

# Combined capacity+ACL transaction: an occupied tail must reject shrink
# BEFORE ACL evaluation can close anybody, even though the candidate denies
# both target peers. Remote-close notification acknowledges the freed tail.
sed 's/maxSessions: 1/maxSessions: 2/' "$CONF_FILE.allowed" >"$CONF_FILE"
activate_policy
exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: acl-yaml-tail-ready\n' >&7 || error_exit 1
wait_content 'acl-yaml-tail-ready' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE.revoked" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
[[ "$reload_status" == *"result=activation_failed active_generation=$generation"* &&
   "$reload_status" == *"source_capability=live_swap"* ]] || error_exit 1 "occupied-tail shrink: $reload_status"
printf '<167>Mar 10 01:00:00 host app: acl-yaml-rollback-first\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: acl-yaml-rollback-tail\n' >&7 || error_exit 1
wait_content 'acl-yaml-rollback-first' "$RSYSLOG_OUT_LOG"
wait_content 'acl-yaml-rollback-tail' "$RSYSLOG_OUT_LOG"
assert_control
close_notices() {
	awk '/closed by remote peer/ { ++count } END { print count + 0 }' "${RSYSLOG_DYNNAME}.started"
}
close_before=$(close_notices)
exec 7>&-
wait_file_lines --count-function close_notices "${RSYSLOG_DYNNAME}.started" "$((close_before + 1))"
activate_policy
IFS= read -r -t 10 -u 9 denied_frame
closed=$?
[[ "$closed" -eq 1 && -z "$denied_frame" ]] || error_exit 1
cp "$CONF_FILE.allowed" "$CONF_FILE"
activate_policy
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: acl-yaml-combined-after\n' >&9 || error_exit 1
wait_content 'acl-yaml-combined-after' "$RSYSLOG_OUT_LOG"
# Removing the module override restores the unchanged legacy fallback.
# The explicit control ACL still wins, and both established peers remain usable.
sed '0,/allowedSender/{/allowedSender/d;}' "$CONF_FILE.allowed" >"$CONF_FILE"
activate_policy
printf '<167>Mar 10 01:00:00 host app: acl-yaml-legacy-fallback\n' >&9 || error_exit 1
wait_content 'acl-yaml-legacy-fallback' "$RSYSLOG_OUT_LOG"
assert_control
exec 8>&-
exec 9>&-
shutdown_when_empty
wait_shutdown
# Drain the action queue before the final negative oracle, so a buggy regular
# close cannot hide a flushed partial frame behind asynchronous output.
assert_content_missing 'acl-yaml-partial'
exit_test
