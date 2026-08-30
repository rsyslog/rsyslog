#!/bin/bash
# Verify RainerScript live reload of imtcp session/control scalars on an
# established TCP session. Visible timestamp offsets prove defaultTZ snapshot
# updates; exact live-swap generations plus records on the same socket prove
# module- and input-level starvation and unnamed rate-limit profiles commit
# without reconnecting. The focused tcpsrv unit verifies limiter propagation.
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

# starvationProtection.maxReads is sampled once per receive dispatch. The
# input fence drains the old dispatch before this module default is published.
sed 's|module(load="../plugins/imtcp/.libs/imtcp")|module(load="../plugins/imtcp/.libs/imtcp" starvationProtection.maxReads="1")|' \
	"$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=4"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript module starvation limit did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: starvation-module-live\n' >&9 || error_exit 1
wait_content 'starvation-module-live' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.module-live"

# An explicit input value overrides the active module default through the same
# descriptor, classifier, fence and commit path.
sed 's/ruleset="main")/ruleset="main" starvationProtection.maxReads="0")/' \
	"$CONF_FILE.module-live" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=5"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript input starvation override did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: starvation-input-live\n' >&9 || error_exit 1
wait_content 'starvation-input-live' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.input-live"

# Unnamed Linux-like rate limiting owns listener-local runtime state. The
# acquired input fence makes resetting that state race-free; this integration
# oracle covers classification, publication, and persistent-session lifetime.
sed 's/ruleset="main"/ruleset="main" ratelimit.interval="60" ratelimit.burst="12000"/' \
	"$CONF_FILE.input-live" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=6"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript unnamed rate limit did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: ratelimit-live\n' >&9 || error_exit 1
wait_content 'ratelimit-live' "$RSYSLOG_OUT_LOG"
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
