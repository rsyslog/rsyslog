#!/bin/bash
# Verify native YAML parity for live imtcp session/control scalar updates. The
# same open TCP stream emits changed/restored timezone offsets and remains
# usable after module- and input-level starvation and unnamed rate-limit
# generations. The focused tcpsrv unit verifies scalar and limiter propagation.
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
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
