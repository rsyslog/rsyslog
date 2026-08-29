#!/bin/bash
# Verify endpoint reconciliation pairs named dynamic imtcp listeners by stable
# config identity rather than source order. The first HUP swaps both input
# blocks while moving an explicit default between them; effective profiles are
# unchanged and must publish as REUSE. A subsequent live change proves prepare
# resolves the reordered source entry back to the correct runtime listener.
# Persistent connections on both original sockets are the session-retention
# oracle; all waits use visible output or the imdiag HUP acknowledgement.
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
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port2"'
add_yaml_conf '    name: second'
add_yaml_conf '    ruleset: main'
add_yaml_conf '    flowControl: "on"'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    actions:'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        name: sink'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
startup
assign_tcpflood_port2 "$RSYSLOG_DYNNAME.tcpflood_port2"
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
exec 8<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT2"
cp "$CONF_FILE" "$CONF_FILE.base"

# Swap the complete endpoint identities across source positions. The explicit
# flowControl:on remains at the second position, so both source nodes change
# while their name-matched effective runtime profiles remain equivalent.
sed -e "s|$RSYSLOG_DYNNAME.tcpflood_port2|$RSYSLOG_DYNNAME.tcpflood_port.swap|" \
	-e "s|$RSYSLOG_DYNNAME.tcpflood_port\"|$RSYSLOG_DYNNAME.tcpflood_port2\"|" \
	-e "s|$RSYSLOG_DYNNAME.tcpflood_port.swap|$RSYSLOG_DYNNAME.tcpflood_port|" \
	-e 's/name: first/name: reload-swap/' \
	-e 's/name: second/name: first/' \
	-e 's/name: reload-swap/name: second/' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2 unchanged=5 added=0 removed=0 modified=2 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: reordered named endpoints were not reconciled as reuse: $reload_status"
	error_exit 1
fi
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=2 unchanged=7 added=0 removed=0 modified=0 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: reordered endpoint baseline was not retained: $reload_status"
	error_exit 1
fi

# The first runtime endpoint is now the second source entry. Changing its
# effective flow-control value must still find and fence it by config name.
sed 's/flowControl: "on"/flowControl: "off"/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3 unchanged=6 added=0 removed=0 modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: reordered runtime endpoint was not resolved for live activation: $reload_status"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: reorder-first\n' >&9; then error_exit 1; fi
if ! printf '<167>Mar 10 01:00:00 host app: reorder-second\n' >&8; then error_exit 1; fi
wait_content 'reorder-first' "$RSYSLOG_OUT_LOG"
wait_content 'reorder-second' "$RSYSLOG_OUT_LOG"

# Config identity must never hide a socket-tuple change. Such a candidate is
# restart-required, and rejection must preserve generation three plus both
# established sessions.
sed "s|$RSYSLOG_DYNNAME.tcpflood_port\"|$RSYSLOG_DYNNAME.tcpflood_port.changed\"|" "$CONF_FILE" \
	>"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=3 unchanged=6 added=0 removed=0 modified=1 invalid=0 source_capability=restart_required"* ]]; then
	echo "FAIL: endpoint change under a stable name was not rejected: $reload_status"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: reorder-survives-reject\n' >&9; then error_exit 1; fi
wait_content 'reorder-survives-reject' "$RSYSLOG_OUT_LOG"

exec 9>&-
exec 8>&-
shutdown_when_empty
wait_shutdown
exit_test
