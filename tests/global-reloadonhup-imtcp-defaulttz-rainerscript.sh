#!/bin/bash
# Verify RainerScript live reload of imtcp defaultTZ on an established TCP
# session. Visible timestamp offsets after each HUP are the oracle that both
# the listener default and the existing session snapshot changed atomically.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf '
global(config.reloadOnHUP="on")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="main")
ruleset(name="main") {
    action(type="omfile" name="tz_sink" file="'$RSYSLOG_OUT_LOG'")
}
'
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: tz-baseline\n' >&9 || error_exit 1
wait_content '01:00:00+00:00 host app: tz-baseline' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.base"

sed 's/ruleset="main")/ruleset="main" defaultTZ="+02:00")/' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript defaultTZ update did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: tz-live\n' >&9 || error_exit 1
wait_content '01:00:00+02:00 host app: tz-live' "$RSYSLOG_OUT_LOG"

cp "$CONF_FILE.base" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript defaultTZ restore did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: tz-restored\n' >&9 || error_exit 1
wait_content '01:00:00+00:00 host app: tz-restored' "$RSYSLOG_OUT_LOG"
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
