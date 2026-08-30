#!/bin/bash
# Verify live numeric allowedSender replacement for RainerScript.  The
# impstats drop counter is the deterministic oracle that a record from the
# already-established session reached the fenced, newly denied policy; a
# later record on the same file descriptor proves reallow without reconnect.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin impstats
export STATSFILE="$RSYSLOG_DYNNAME.stats"
generate_conf
add_conf '
global(config.reloadOnHUP="on")
module(load="../plugins/imtcp/.libs/imtcp" allowedSender=["127.0.0.1/32"])
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" interval="1")
input(type="imtcp" name="acl-rs" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
if $msg contains "acl-rs" then action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: acl-rs-before\n' >&9 || error_exit 1
wait_content 'acl-rs-before' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.allowed"

sed 's/127\.0\.0\.1\/32/192.0.2.1\/32/' "$CONF_FILE.allowed" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: numeric RainerScript ACL did not activate live: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: acl-rs-denied\n' >&9 || error_exit 1
wait_content 'reload_acl_message_dropped_total=1' "$STATSFILE"
assert_content_missing 'acl-rs-denied' "$RSYSLOG_OUT_LOG"

cp "$CONF_FILE.allowed" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: numeric RainerScript ACL reallow did not activate live: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: acl-rs-after\n' >&9 || error_exit 1
wait_content 'acl-rs-after' "$RSYSLOG_OUT_LOG"

# An input-local ACL overrides the restored module default through the same
# live path.  The second counter increment proves the established session was
# re-evaluated against the input value rather than the inherited module list.
sed 's/listenPortFileName="\([^\"]*\)")/listenPortFileName="\1" allowedSender=["192.0.2.1\/32"])/' \
	"$CONF_FILE.allowed" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=4"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript input ACL override did not activate live: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: acl-rs-input-denied\n' >&9 || error_exit 1
wait_content 'reload_acl_message_dropped_total=2' "$STATSFILE"
assert_content_missing 'acl-rs-input-denied' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE.allowed" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=5"* ]]; then
	echo "FAIL: RainerScript input ACL override was not removed live: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: acl-rs-final\n' >&9 || error_exit 1
wait_content 'acl-rs-final' "$RSYSLOG_OUT_LOG"

# Hostname ACLs intentionally remain outside the live slice because their
# preparation would require DNS and generation-owned resolution state.  The
# rejected HUP must retain both generation 5 and the established session's
# restored numeric policy.
sed 's/127\.0\.0\.1\/32/\*.example.invalid/' "$CONF_FILE.allowed" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=5"* ||
      "$reload_status" != *"source_capability=restart_required"* ]]; then
	echo "FAIL: hostname ACL did not reject atomically: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: acl-rs-after-reject\n' >&9 || error_exit 1
wait_content 'acl-rs-after-reject' "$RSYSLOG_OUT_LOG"
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
