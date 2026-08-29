#!/bin/bash
# Verify that a RainerScript imtcp input can move an established TCP session
# between two existing ruleset shells. Distinct output files after each HUP
# prove the cached session pointer changed; TCP reconnects cannot satisfy it.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf '
global(config.reloadOnHUP="on")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="main")
ruleset(name="main") {
    action(type="omfile" name="main_sink" file="'$RSYSLOG_OUT_LOG'")
}
ruleset(name="alternate") {
    action(type="omfile" name="alternate_sink" file="'$RSYSLOG2_OUT_LOG'")
}
'
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: route-main-before\n' >&9 || error_exit 1
wait_content 'route-main-before' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.base"

sed 's/ruleset="main")/ruleset="alternate")/' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript session ruleset update did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: route-alternate\n' >&9 || error_exit 1
wait_content 'route-alternate' "$RSYSLOG2_OUT_LOG"

cp "$CONF_FILE.base" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript session ruleset restore did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: route-main-after\n' >&9 || error_exit 1
wait_content 'route-main-after' "$RSYSLOG_OUT_LOG"
assert_content_missing 'route-alternate' "$RSYSLOG_OUT_LOG"
custom_assert_content_missing 'route-main-after' "$RSYSLOG2_OUT_LOG"
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
