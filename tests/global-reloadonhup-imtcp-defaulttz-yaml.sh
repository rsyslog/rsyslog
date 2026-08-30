#!/bin/bash
# Verify native YAML parity for live imtcp session/control scalar updates. The
# same open TCP stream emits changed/restored timezone offsets and remains
# usable after module- and input-level starvation plus unnamed and named
# rate-limit generations. Exact accepted counts and fresh drop diagnostics
# prove policy add, update, removal, and limiter ownership transfer while the
# same stream remains usable. The focused tcpsrv unit verifies scalar and
# limiter-pointer propagation.
# A constant RainerScript include only routes internal diagnostics to a test
# file; every imtcp setting and reload candidate remains native YAML.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp
generate_conf --yaml-only
sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "on"' "${TESTCONF_NM}.yaml"
printf 'action(type="omfile" file="%s")\n' "$RSYSLOG_DYNNAME.started" >"$RSYSLOG_DYNNAME.internal.conf"
add_yaml_conf 'include:'
add_yaml_conf '  - path: "'$RSYSLOG_DYNNAME'.internal.conf"'
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf 'ratelimits:'
add_yaml_conf '  - name: policy_tight'
add_yaml_conf '    interval: 60'
add_yaml_conf '    burst: 1'
add_yaml_conf '  - name: policy_wide'
add_yaml_conf '    interval: 60'
add_yaml_conf '    burst: 100'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"'
add_yaml_conf '    ruleset: main'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    actions:'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        name: tz_sink'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: tz-baseline\n' >&9 || error_exit 1
wait_content '01:00:00+00:00 host app: tz-baseline' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.base"

sed '/ruleset: main/a\    defaultTZ: "+02:00"' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: YAML defaultTZ update did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: tz-live\n' >&9 || error_exit 1
wait_content '01:00:00+02:00 host app: tz-live' "$RSYSLOG_OUT_LOG"

cp "$CONF_FILE.base" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: YAML defaultTZ restore did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: tz-restored\n' >&9 || error_exit 1
wait_content '01:00:00+00:00 host app: tz-restored' "$RSYSLOG_OUT_LOG"

# The module default exercises native-YAML effective-profile classification and
# persistent-session publication. The focused unit checks the fenced scalar
# update directly instead of inferring receive-dispatch behavior from timing.
sed '/load: "..\/plugins\/imtcp\/.libs\/imtcp"/a\    starvationProtection.maxReads: 1' \
	"$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=4"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: YAML module starvation limit did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: starvation-module-live\n' >&9 || error_exit 1
wait_content 'starvation-module-live' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.module-live"

# A per-input override exercises native YAML input lowering and the same live
# scalar commit while the established connection remains open.
sed '/ruleset: main/a\    starvationProtection.maxReads: 0' "$CONF_FILE.module-live" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=5"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: YAML input starvation override did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: starvation-input-live\n' >&9 || error_exit 1
wait_content 'starvation-input-live' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.input-live"

# Five frames in one TCP write exercise the live limiter directly. Exactly
# three records plus the first-drop diagnostic prove that the remaining frames
# reached the limiter; this is a state oracle rather than an elapsed-time test.
sed '/ruleset: main/a\    ratelimit.interval: 60\n    ratelimit.burst: 3' \
	"$CONF_FILE.input-live" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=6"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: YAML unnamed rate limit did not activate: $reload_status"
	error_exit 1
fi
printf '%s\n' \
	'<167>Mar 10 01:00:00 host app: ratelimit-live-1' \
	'<167>Mar 10 01:00:00 host app: ratelimit-live-2' \
	'<167>Mar 10 01:00:00 host app: ratelimit-live-3' \
	'<167>Mar 10 01:00:00 host app: ratelimit-live-4' \
	'<167>Mar 10 01:00:00 host app: ratelimit-live-5' >&9 || error_exit 1
wait_content 'ratelimit-live-3' "$RSYSLOG_OUT_LOG"
wait_content 'begin to drop messages due to rate-limiting' "$RSYSLOG_DYNNAME.started"
wait_queueempty
content_count_check 'ratelimit-live-' 3 "$RSYSLOG_OUT_LOG"
check_not_present 'ratelimit-live-4' "$RSYSLOG_OUT_LOG"
check_not_present 'ratelimit-live-5' "$RSYSLOG_OUT_LOG"

# Replace the unnamed bucket with a privately prepared named limiter. Clearing
# the internal sink first makes its next first-drop diagnostic synchronize the
# negative assertion without relying on elapsed time.
cp "$CONF_FILE" "$CONF_FILE.unnamed-live"
: >"$RSYSLOG_DYNNAME.started"
sed -e '/    ratelimit.interval: 60/d' \
	-e 's/    ratelimit.burst: 3/    ratelimit.name: policy_tight/' \
	"$CONF_FILE.unnamed-live" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=7"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: YAML named tight rate limit did not activate: $reload_status"
	error_exit 1
fi
printf '%s\n' \
	'<167>Mar 10 01:00:00 host named-tight: named-tight-accepted' \
	'<167>Mar 10 01:00:00 host named-tight: named-tight-dropped' >&9 || error_exit 1
wait_content 'named-tight-accepted' "$RSYSLOG_OUT_LOG"
wait_content 'begin to drop messages due to rate-limiting' "$RSYSLOG_DYNNAME.started"
wait_queueempty
content_count_check 'named-tight-' 1 "$RSYSLOG_OUT_LOG"
check_not_present 'named-tight-dropped' "$RSYSLOG_OUT_LOG"

# Swap to a second named policy while retaining the accepted connection.
sed 's/    ratelimit.name: policy_tight/    ratelimit.name: policy_wide/' "$CONF_FILE" >"$CONF_FILE.wide"
mv "$CONF_FILE.wide" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=8"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: YAML named wide rate limit did not activate: $reload_status"
	error_exit 1
fi
printf '%s\n' \
	'<167>Mar 10 01:00:00 host named-wide: named-wide-1' \
	'<167>Mar 10 01:00:00 host named-wide: named-wide-2' \
	'<167>Mar 10 01:00:00 host named-wide: named-wide-3' >&9 || error_exit 1
wait_content 'named-wide-3' "$RSYSLOG_OUT_LOG"
wait_queueempty
content_count_check 'named-wide-' 3 "$RSYSLOG_OUT_LOG"

# Exercise the reverse ownership transfer: the prepared standalone limiter has
# no name, while the retired named policy and its string survive until Resume.
: >"$RSYSLOG_DYNNAME.started"
sed 's/    ratelimit.name: policy_wide/    ratelimit.interval: 60\
    ratelimit.burst: 2/' "$CONF_FILE" >"$CONF_FILE.unnamed-again"
mv "$CONF_FILE.unnamed-again" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=9"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: YAML unnamed rate limit restore did not activate: $reload_status"
	error_exit 1
fi
printf '%s\n' \
	'<167>Mar 10 01:00:00 host unnamed-again: unnamed-again-1' \
	'<167>Mar 10 01:00:00 host unnamed-again: unnamed-again-2' \
	'<167>Mar 10 01:00:00 host unnamed-again: unnamed-again-3' >&9 || error_exit 1
wait_content 'unnamed-again-2' "$RSYSLOG_OUT_LOG"
wait_content 'begin to drop messages due to rate-limiting' "$RSYSLOG_DYNNAME.started"
wait_queueempty
content_count_check 'unnamed-again-' 2 "$RSYSLOG_OUT_LOG"
check_not_present 'unnamed-again-3' "$RSYSLOG_OUT_LOG"

# Add a simple named policy and bind it in the same native-YAML candidate. The
# active module snapshot owns the shared bucket for the committed generation.
: >"$RSYSLOG_DYNNAME.started"
sed -e '/  - name: policy_wide/i\  - name: policy_added\
    interval: 60\
    burst: 1' \
	-e '/    ratelimit.interval: 60/d' \
	-e 's/    ratelimit.burst: 2/    ratelimit.name: policy_added/' \
	"$CONF_FILE" >"$CONF_FILE.added-policy"
mv "$CONF_FILE.added-policy" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=10"* ||
      "$reload_status" != *"added=1 removed=0 modified=1"* ||
      "$reload_status" != *"invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: YAML added named rate limit did not activate: $reload_status"
	error_exit 1
fi
printf '%s\n' \
	'<167>Mar 10 01:00:00 host added-policy: added-policy-accepted' \
	'<167>Mar 10 01:00:00 host added-policy: added-policy-dropped' >&9 || error_exit 1
wait_content 'added-policy-accepted' "$RSYSLOG_OUT_LOG"
wait_content 'begin to drop messages due to rate-limiting' "$RSYSLOG_DYNNAME.started"
wait_queueempty
content_count_check 'added-policy-' 1 "$RSYSLOG_OUT_LOG"
check_not_present 'added-policy-dropped' "$RSYSLOG_OUT_LOG"

# Publish an effective-default-only source change as REUSE. The runtime still
# references the preceding active snapshot and its policy registry.
sed '/    ratelimit.name: policy_added/a\    flowControl: "on"' "$CONF_FILE" >"$CONF_FILE.reuse"
mv "$CONF_FILE.reuse" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=11"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: YAML source-equivalent policy generation did not activate: $reload_status"
	error_exit 1
fi

# Updating an imtcp-exclusive simple named definition prepares a fresh shared
# bucket and swaps it at the same safepoint. Exactly two accepted frames prove
# that the new burst is active; the drop diagnostic orders the negative oracle.
: >"$RSYSLOG_DYNNAME.started"
sed '/  - name: policy_added/{n;n;s/burst: 1/burst: 2/;}' "$CONF_FILE" >"$CONF_FILE.changed-policy"
mv "$CONF_FILE.changed-policy" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=12"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: YAML changed named rate limit did not activate: $reload_status"
	error_exit 1
fi
printf '%s\n' \
	'<167>Mar 10 01:00:00 host changed-policy: changed-policy-1' \
	'<167>Mar 10 01:00:00 host changed-policy: changed-policy-2' \
	'<167>Mar 10 01:00:00 host changed-policy: changed-policy-3' >&9 || error_exit 1
wait_content 'changed-policy-2' "$RSYSLOG_OUT_LOG"
wait_content 'begin to drop messages due to rate-limiting' "$RSYSLOG_DYNNAME.started"
wait_queueempty
content_count_check 'changed-policy-' 2 "$RSYSLOG_OUT_LOG"
check_not_present 'changed-policy-3' "$RSYSLOG_OUT_LOG"

# Remove the imtcp-exclusive definition while atomically switching its last
# listener to a prepared unnamed limiter. The same TCP stream then observes
# exactly the new three-message bucket, proving safe registry retirement.
: >"$RSYSLOG_DYNNAME.started"
sed -e '/  - name: policy_added/{N;N;d;}' \
	-e '/    ratelimit.name: policy_added/d' \
	-e '/    flowControl: "on"/a\    ratelimit.interval: 60\
    ratelimit.burst: 3' \
	"$CONF_FILE" >"$CONF_FILE.removed-policy"
mv "$CONF_FILE.removed-policy" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=13"* ||
      "$reload_status" != *"added=0 removed=1 modified=1"* ||
      "$reload_status" != *"invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: YAML removed named rate limit did not activate: $reload_status"
	error_exit 1
fi
printf '%s\n' \
	'<167>Mar 10 01:00:00 host removed-policy: removed-policy-1' \
	'<167>Mar 10 01:00:00 host removed-policy: removed-policy-2' \
	'<167>Mar 10 01:00:00 host removed-policy: removed-policy-3' \
	'<167>Mar 10 01:00:00 host removed-policy: removed-policy-4' >&9 || error_exit 1
wait_content 'removed-policy-3' "$RSYSLOG_OUT_LOG"
wait_content 'begin to drop messages due to rate-limiting' "$RSYSLOG_DYNNAME.started"
wait_queueempty
content_count_check 'removed-policy-' 3 "$RSYSLOG_OUT_LOG"
check_not_present 'removed-policy-4' "$RSYSLOG_OUT_LOG"
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
