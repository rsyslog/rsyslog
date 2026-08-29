#!/bin/bash
# Verify that a named imtcp endpoint can be prepared and published by HUP.
# The original TCP session remains open across activation. A connection to the
# newly configured fixed port plus visible records from both persistent sockets
# are the publication and session-retention oracles. Removal must stop new
# accepts while the already established session keeps delivering. After that
# session closes, bounded HUP/status retries prove asynchronous drain retirement
# completes without changing the activated generation. The testbench-selected
# free port is only stimulus; production prepare validates the bind pre-commit.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf 'global(config.reloadOnHUP="on")'
add_conf 'module(load="../plugins/imtcp/.libs/imtcp")'
add_conf 'input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" name="first" ruleset="main")'
add_conf 'ruleset(name="main") {'
add_conf '  action(type="omfile" name="sink" file="'$RSYSLOG_OUT_LOG'")'
add_conf '}'
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
cp "$CONF_FILE" "$CONF_FILE.base"
ADDED_PORT="$(get_free_port)"

sed '/name="first"/a input(type="imtcp" port="'$ADDED_PORT'" name="second" ruleset="main")' \
	"$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"added=1 removed=0"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: added endpoint was not activated: $reload_status"
	error_exit 1
fi
cp "$CONF_FILE" "$CONF_FILE.added"
exec 8<>"/dev/tcp/127.0.0.1/$ADDED_PORT"
if ! printf '<167>Mar 10 01:00:00 host app: add-first\n' >&9; then error_exit 1; fi
if ! printf '<167>Mar 10 01:00:00 host app: add-second\n' >&8; then error_exit 1; fi
wait_content 'add-first' "$RSYSLOG_OUT_LOG"
wait_content 'add-second' "$RSYSLOG_OUT_LOG"

# Preparing another endpoint on imdiag's already-bound loopback port must fail
# before publication. Generation two and both established imtcp sessions are
# the rollback oracle; no timing or external helper listener is involved.
sed '/name="second"/a input(type="imtcp" address="127.0.0.1" port="'$IMDIAG_PORT'" name="bind-conflict" ruleset="main")' \
	"$CONF_FILE.added" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activation_failed active_generation=2"* ||
      "$reload_status" != *"added=1"* ]]; then
	echo "FAIL: listener bind failure did not roll back prepare: $reload_status"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: add-survives-bind-failure\n' >&8; then error_exit 1; fi
wait_content 'add-survives-bind-failure' "$RSYSLOG_OUT_LOG"

cp "$CONF_FILE.base" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"removed=1"* ||
      "$reload_status" != *"source_capability=drain_replace"* ]]; then
	echo "FAIL: endpoint removal was not activated: $reload_status"
	error_exit 1
fi
if (exec 7<>"/dev/tcp/127.0.0.1/$ADDED_PORT") 2>/dev/null; then
	echo "FAIL: removed endpoint still accepts new connections"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: remove-existing-session-survives\n' >&8; then error_exit 1; fi
wait_content 'remove-existing-session-survives' "$RSYSLOG_OUT_LOG"

exec 8>&-
for ((retire_try = 1; retire_try <= 20; ++retire_try)); do
	issue_HUP
	reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
	if [[ "$reload_status" == *"result=reported_only active_generation=3"* ]]; then break; fi
	if [[ "$reload_status" != *"result=activation_failed active_generation=3"* ]]; then
		echo "FAIL: unexpected status while retiring drained endpoint: $reload_status"
		error_exit 1
	fi
done
if [[ "$reload_status" != *"result=reported_only active_generation=3"* ]]; then
	echo "FAIL: removed endpoint did not finish retirement after $((retire_try - 1)) attempts: $reload_status"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: original-after-remove-retire\n' >&9; then error_exit 1; fi
wait_content 'original-after-remove-retire' "$RSYSLOG_OUT_LOG"

exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
