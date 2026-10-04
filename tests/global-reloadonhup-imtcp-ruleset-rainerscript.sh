#!/bin/bash
# Verify that a RainerScript imtcp input can move an established TCP session
# between two existing ruleset shells. Distinct output files after each HUP
# prove the cached session pointer changed; TCP reconnects cannot satisfy it.
# Also permit a same-header script edit, but reject queue/parser/unknown header
# edits, alone or with a script change. Status keeps generation 4; records on
# the established session prove rejected scripts never replaced the live body.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf '
global(config.reloadOnHUP="on")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="main")
ruleset(name="main") {
    set $.reloadControl = 0;
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

sed 's/set $.reloadControl = 0;/set $.reloadControl = 1;/' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=4"* ]]; then
	echo "FAIL: unchanged ruleset header blocked script edit: $reload_status"
	error_exit 1
fi
cp "$CONF_FILE" "$CONF_FILE.body"
for header in 'queue.type="LinkedList"' 'parser="rsyslog.rfc3164"' 'unknown.parameter="ignored"'; do
	for combined in 0 1; do
		sed "s/ruleset(name=\"main\")/ruleset(name=\"main\" $header)/" "$CONF_FILE.body" >"$CONF_FILE.next"
		if [[ "$combined" == 1 ]]; then
			sed 's/set $.reloadControl = 1;/if $msg contains "header-body-probe" then stop/' \
				"$CONF_FILE.next" >"$CONF_FILE"
		else
			cp "$CONF_FILE.next" "$CONF_FILE"
		fi
		issue_HUP
		reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
		if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=4"* ]]; then
			echo "FAIL: ruleset metadata edit activated ($header, combined=$combined): $reload_status"
			error_exit 1
		fi
		probe="header-body-probe-${header%%=*}-$combined"
		printf '<167>Mar 10 01:00:00 host app: %s\n' "$probe" >&9 || error_exit 1
		wait_content "$probe" "$RSYSLOG_OUT_LOG"
	done
done
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
