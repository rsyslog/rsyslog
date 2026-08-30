#!/bin/bash
# Native-YAML parity for transactional replacement of a named imtcp endpoint.
# A temporary seed obtains the future fixed port without a free-port race and
# is retired first. The persistent old session, a new fixed-port connection,
# failed connects to retired accept sockets, and bounded retirement status are
# the content/state oracles; no correctness assertion depends on a sleep.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp
generate_conf --yaml-only
sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "on"' "${TESTCONF_NM}.yaml"
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"'
add_yaml_conf '    name: replace'
add_yaml_conf '    ruleset: main'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    address: 127.0.0.1'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port2"'
add_yaml_conf '    name: seed'
add_yaml_conf '    ruleset: main'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    actions:'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        name: sink'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
startup
assign_tcpflood_port2 "$RSYSLOG_DYNNAME.tcpflood_port2"
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
REPLACEMENT_PORT="$TCPFLOOD_PORT2"
cp "$CONF_FILE" "$CONF_FILE.startup"

awk '
    /^  - type: imtcp$/ { ++input_index }
    input_index == 2 { if (/^    ruleset: main$/) input_index = 0; next }
    { print }
' "$CONF_FILE.startup" >"$CONF_FILE.base"
# The range expression above deliberately removes only the second input. Its
# structure is asserted so a future YAML layout change cannot silently weaken
# the replacement stimulus.
if [[ "$(grep -c '^  - type: imtcp$' "$CONF_FILE.base")" -ne 1 ]]; then
	echo "FAIL: YAML seed removal did not leave exactly one imtcp input"
	error_exit 1
fi
cp "$CONF_FILE.base" "$CONF_FILE"
issue_HUP
# Listener-thread retirement may finish just after HUP publication. The polls
# only bound hangs; stable generation 2 with pending 1 -> 0 proves completion.
for ((seed_retire_try = 1; seed_retire_try <= 50; ++seed_retire_try)); do
	reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
	if [[ "$reload_status" == *"result=activated active_generation=2"* &&
	      "$reload_status" == *"removed=1"* &&
	      "$reload_status" == *"source_capability=drain_replace"* &&
	      "$reload_status" == *"retirement_pending=0"* ]]; then break; fi
	if [[ "$reload_status" != *"result=activated active_generation=2"* ||
	      "$reload_status" != *"removed=1"* ||
	      "$reload_status" != *"source_capability=drain_replace"* ||
	      "$reload_status" != *"retirement_pending=1"* ]]; then
		echo "FAIL: unexpected YAML status while retiring replacement seed: $reload_status"
		error_exit 1
	fi
	"$TESTTOOL_DIR/msleep" 100
done
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"retirement_pending=0"* ]]; then
	echo "FAIL: YAML replacement seed did not finish retirement after $((seed_retire_try - 1)) attempts: $reload_status"
	error_exit 1
fi
if (exec 7<>"/dev/tcp/127.0.0.1/$REPLACEMENT_PORT") 2>/dev/null; then
	echo "FAIL: retired YAML seed still accepts connections"
	error_exit 1
fi

sed -e '/listenPortFileName: .*tcpflood_port"/d' \
	-e '0,/port: "0"/s//address: 127.0.0.1\n    port: "'$REPLACEMENT_PORT'"/' \
	"$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"modified=1 invalid=0"* ||
      "$reload_status" != *"source_capability=drain_replace"* ||
      "$reload_status" != *"retirement_pending=1"* ]]; then
	echo "FAIL: YAML fixed endpoint replacement was not activated: $reload_status"
	error_exit 1
fi
if (exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT") 2>/dev/null; then
	echo "FAIL: replaced YAML listener still accepts new sessions"
	error_exit 1
fi
exec 8<>"/dev/tcp/127.0.0.1/$REPLACEMENT_PORT"
if ! printf '<167>Mar 10 01:00:00 host app: yaml-replace-old-session\n' >&9; then error_exit 1; fi
if ! printf '<167>Mar 10 01:00:00 host app: yaml-replace-new-listener\n' >&8; then error_exit 1; fi
wait_content 'yaml-replace-old-session' "$RSYSLOG_OUT_LOG"
wait_content 'yaml-replace-new-listener' "$RSYSLOG_OUT_LOG"

exec 9>&-
for ((retire_try = 1; retire_try <= 50; ++retire_try)); do
	reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
	if [[ "$reload_status" == *"result=activated active_generation=3"* &&
	      "$reload_status" == *"retirement_pending=0"* ]]; then break; fi
	"$TESTTOOL_DIR/msleep" 100
done
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"retirement_pending=0"* ]]; then
	echo "FAIL: YAML replaced endpoint did not retire: $reload_status"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: yaml-replace-after-retire\n' >&8; then error_exit 1; fi
wait_content 'yaml-replace-after-retire' "$RSYSLOG_OUT_LOG"

exec 8>&-
shutdown_when_empty
wait_shutdown
exit_test
