#!/bin/bash
# Verify native YAML reaches the same transactional imtcp endpoint-add path as
# RainerScript. A connection to the new fixed port plus records from both
# persistent sockets prove publication and session retention. Rewriting that
# port with leading zeroes proves canonical endpoint matching. Removal stops new
# accepts while the old session stays usable. After closing it, bounded 100 ms
# status polling proves automatic retirement without another HUP. The 50 polls
# are only a hang bound; retirement_pending=0 is the completion oracle.
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
add_yaml_conf '    name: first'
add_yaml_conf '    ruleset: main'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    actions:'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        name: sink'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
cp "$CONF_FILE" "$CONF_FILE.base"
ADDED_PORT="$(get_free_port)"

sed '/^rulesets:/i\  - type: imtcp\
    address: "127.0.0.1"\
    port: "'$ADDED_PORT'"\
    name: second\
    ruleset: main' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"added=1 removed=0"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: YAML added endpoint was not activated: $reload_status"
	error_exit 1
fi
exec 8<>"/dev/tcp/127.0.0.1/$ADDED_PORT"
if ! printf '<167>Mar 10 01:00:00 host app: yaml-add-first\n' >&9; then error_exit 1; fi
if ! printf '<167>Mar 10 01:00:00 host app: yaml-add-second\n' >&8; then error_exit 1; fi
wait_content 'yaml-add-first' "$RSYSLOG_OUT_LOG"
wait_content 'yaml-add-second' "$RSYSLOG_OUT_LOG"

# A source-level spelling change of the same numeric port must reuse the
# listener and both established sessions rather than demand a restart.
sed 's/port: "'$ADDED_PORT'"/port: "0'$ADDED_PORT'"/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"added=0 removed=0 modified=1 invalid=0"* ||
      "$reload_status" != *"source_capability=reuse"* ]]; then
	echo "FAIL: YAML canonical endpoint spelling was not reused: $reload_status"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: yaml-canonical-endpoint-session\n' >&8; then error_exit 1; fi
wait_content 'yaml-canonical-endpoint-session' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.added"

# The already-bound imdiag port makes listener preparation fail
# deterministically. Generation three and both existing sessions prove that
# the YAML candidate was rolled back before publication.
sed '/^rulesets:/i\  - type: imtcp\
    address: "127.0.0.1"\
    port: "'$IMDIAG_PORT'"\
    name: bind-conflict\
    ruleset: main' "$CONF_FILE.added" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activation_failed active_generation=3"* ||
      "$reload_status" != *"added=1"* ]]; then
	echo "FAIL: YAML listener bind failure did not roll back prepare: $reload_status"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: yaml-add-survives-bind-failure\n' >&8; then error_exit 1; fi
wait_content 'yaml-add-survives-bind-failure' "$RSYSLOG_OUT_LOG"

cp "$CONF_FILE.base" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=4"* ||
      "$reload_status" != *"removed=1"* ||
      "$reload_status" != *"source_capability=drain_replace"* ||
      "$reload_status" != *"retirement_pending=1"* ]]; then
	echo "FAIL: YAML endpoint removal was not activated: $reload_status"
	error_exit 1
fi
if (exec 7<>"/dev/tcp/127.0.0.1/$ADDED_PORT") 2>/dev/null; then
	echo "FAIL: YAML removed endpoint still accepts new connections"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: yaml-remove-existing-session-survives\n' >&8; then error_exit 1; fi
wait_content 'yaml-remove-existing-session-survives' "$RSYSLOG_OUT_LOG"

exec 8>&-
for ((retire_try = 1; retire_try <= 50; ++retire_try)); do
	reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
	if [[ "$reload_status" == *"result=activated active_generation=4"* &&
	      "$reload_status" == *"retirement_pending=0"* ]]; then break; fi
	if [[ "$reload_status" != *"result=activated active_generation=4"* ||
	      "$reload_status" != *"retirement_pending=1"* ]]; then
		echo "FAIL: unexpected YAML status while retiring drained endpoint: $reload_status"
		error_exit 1
	fi
	"$TESTTOOL_DIR/msleep" 100
done
if [[ "$reload_status" != *"result=activated active_generation=4"* ||
      "$reload_status" != *"retirement_pending=0"* ]]; then
	echo "FAIL: YAML removed endpoint did not finish retirement after $((retire_try - 1)) attempts: $reload_status"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: yaml-original-after-remove-retire\n' >&9; then error_exit 1; fi
wait_content 'yaml-original-after-remove-retire' "$RSYSLOG_OUT_LOG"

exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
