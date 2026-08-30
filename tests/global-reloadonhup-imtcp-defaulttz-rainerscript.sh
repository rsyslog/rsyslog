#!/bin/bash
# Verify RainerScript live reload of imtcp session/control scalars on an
# established TCP session. Visible timestamp offsets prove defaultTZ snapshot
# updates; exact live-swap generations plus records on the same socket prove
# module- and input-level starvation plus unnamed and named rate-limit profiles
# publish without reconnecting. Exact accepted counts and fresh first-drop
# diagnostics prove policy add, update, removal, and limiter ownership transfer
# while the same TCP stream remains usable. The focused tcpsrv unit verifies
# the scalar and pointer propagation performed by the fenced commit helper.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf '
global(config.reloadOnHUP="on")
module(load="../plugins/imtcp/.libs/imtcp")
ratelimit(name="policy_tight" interval="60" burst="1")
ratelimit(name="policy_wide" interval="60" burst="100")
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

# The module starvation setting exercises effective-profile classification and
# persistent-session publication. The focused unit directly checks the fenced
# scalar update; this shell oracle does not infer dispatch counts from timing.
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

# Unnamed Linux-like rate limiting owns listener-local runtime state. A burst
# of five frames in one TCP write must emit exactly three records plus the
# limiter's first-drop diagnostic. That diagnostic is the synchronization
# oracle proving the remaining frames reached the limiter; no sleep is used.
sed 's/ruleset="main"/ruleset="main" ratelimit.interval="60" ratelimit.burst="3"/' \
	"$CONF_FILE.input-live" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=6"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript unnamed rate limit did not activate: $reload_status"
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

# Switching from the listener-local limiter to a named policy prepares a new
# instance before the fence and swaps its ownership at commit. Truncating the
# internal diagnostic sink first makes the next first-drop message a fresh
# state oracle proving the second frame reached the new one-message bucket.
cp "$CONF_FILE" "$CONF_FILE.unnamed-live"
: >"$RSYSLOG_DYNNAME.started"
sed 's/ratelimit.interval="60" ratelimit.burst="3"/ratelimit.name="policy_tight"/' \
	"$CONF_FILE.unnamed-live" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=7"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript named tight rate limit did not activate: $reload_status"
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

# A second prepared swap resets listener-local bucket state and keeps the same
# accepted session alive. All three frames fit the wide policy.
sed 's/ratelimit[.]name="policy_tight"/ratelimit.name="policy_wide"/' "$CONF_FILE" >"$CONF_FILE.wide"
mv "$CONF_FILE.wide" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=8"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript named wide rate limit did not activate: $reload_status"
	error_exit 1
fi
printf '%s\n' \
	'<167>Mar 10 01:00:00 host named-wide: named-wide-1' \
	'<167>Mar 10 01:00:00 host named-wide: named-wide-2' \
	'<167>Mar 10 01:00:00 host named-wide: named-wide-3' >&9 || error_exit 1
wait_content 'named-wide-3' "$RSYSLOG_OUT_LOG"
wait_queueempty
content_count_check 'named-wide-' 3 "$RSYSLOG_OUT_LOG"

# The reverse transition owns no prepared name but must retire the current one
# after the fence. A fresh two-message unnamed bucket proves that direction of
# the pointer swap while the established stream remains usable.
: >"$RSYSLOG_DYNNAME.started"
sed 's/ratelimit[.]name="policy_wide"/ratelimit.interval="60" ratelimit.burst="2"/' \
	"$CONF_FILE" >"$CONF_FILE.unnamed-again"
mv "$CONF_FILE.unnamed-again" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=9"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript unnamed rate limit restore did not activate: $reload_status"
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

# A new simple named policy can be declared and bound by imtcp in one
# transaction. The candidate owns its private shared bucket for the active
# generation; a fresh drop diagnostic proves the listener uses that bucket.
: >"$RSYSLOG_DYNNAME.started"
sed -e '/ratelimit(name="policy_wide"/a\ratelimit(name="policy_added" interval="60" burst="1")' \
	-e 's/ratelimit.interval="60" ratelimit.burst="2"/ratelimit.name="policy_added"/' \
	"$CONF_FILE" >"$CONF_FILE.added-policy"
mv "$CONF_FILE.added-policy" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=10"* ||
      "$reload_status" != *"added=1 removed=0 modified=1"* ||
      "$reload_status" != *"invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript added named rate limit did not activate: $reload_status"
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

# An explicit effective default advances the source graph as REUSE but must
# retain the module snapshot that owns the active policy bucket.
sed 's/ratelimit[.]name="policy_added"/ratelimit.name="policy_added" flowControl="on"/' \
	"$CONF_FILE" >"$CONF_FILE.reuse"
mv "$CONF_FILE.reuse" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=11"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: RainerScript source-equivalent policy generation did not activate: $reload_status"
	error_exit 1
fi

# Updating an imtcp-exclusive simple named definition prepares a fresh shared
# bucket and swaps it at the same safepoint. Exactly two accepted frames prove
# that the new burst is active; the drop diagnostic orders the negative oracle.
: >"$RSYSLOG_DYNNAME.started"
sed 's/name="policy_added" interval="60" burst="1"/name="policy_added" interval="60" burst="2"/' \
	"$CONF_FILE" >"$CONF_FILE.changed-policy"
mv "$CONF_FILE.changed-policy" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=12"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript changed named rate limit did not activate: $reload_status"
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

# Removing an imtcp-exclusive simple policy is safe once the same candidate
# switches its last listener to a privately prepared unnamed limiter. The
# persistent stream and exact three-message bucket prove the removal and
# limiter ownership transfer completed at one safepoint.
: >"$RSYSLOG_DYNNAME.started"
sed -e '/ratelimit(name="policy_added"/d' \
	-e 's/ratelimit[.]name="policy_added" flowControl="on"/ratelimit.interval="60" ratelimit.burst="3" flowControl="on"/' \
	"$CONF_FILE" >"$CONF_FILE.removed-policy"
mv "$CONF_FILE.removed-policy" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=13"* ||
      "$reload_status" != *"added=0 removed=1 modified=1"* ||
      "$reload_status" != *"invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript removed named rate limit did not activate: $reload_status"
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
