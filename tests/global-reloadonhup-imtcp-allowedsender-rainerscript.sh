#!/bin/bash
# Verify fenced RainerScript allowedSender reload closes newly denied sessions
# and immediately reclaims their maxSessions slot.  The policy is first
# widened so the established session remains usable, then narrowed to revoke
# it while a partial non-LF frame is buffered.  The absence of that partial
# frame after the fence proves error-close semantics (no PrepareClose flush),
# and a new connection after reallow proves the slot and epoll descriptor were
# reclaimed synchronously.  A second, still-permitted message is the
# compatibility control for established-session retention.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin impstats
export STATSFILE="$RSYSLOG_DYNNAME.stats"
generate_conf
add_conf '
global(config.reloadOnHUP="on" net.aclResolveHostname="off")
module(load="../plugins/imtcp/.libs/imtcp" maxSessions="1" allowedSender=["127.0.0.1/32"])
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" interval="1")
input(type="imtcp" name="acl-rs" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
if $msg contains "acl-rs" then action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: acl-rs-before\n' >&9 || error_exit 1
wait_content 'acl-rs-before' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.allowed"

# A changed policy that still permits localhost must retain the stream.
sed 's/allowedSender=\["127\.0\.0\.1\/32"\]/allowedSender=["127.0.0.1\/32","192.0.2.1\/32"]/' \
	"$CONF_FILE.allowed" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript ACL expansion did not activate live: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: acl-rs-survives\n' >&9 || error_exit 1
wait_content 'acl-rs-survives' "$RSYSLOG_OUT_LOG"

# Ensure the revoked session has consumed a partial frame before the fence.
# The imtcp byte counter is cumulative; waiting for it to increase after the
# write is the state oracle that the non-LF bytes reached imtcp without being
# submitted as a message.
wait_content 'origin=imtcp.*bytes.received=' "$STATSFILE"
partial_bytes_before=$(awk -F'bytes.received=' '/origin=imtcp/ && /bytes.received=/ { split($2, v, /[^0-9]/); value=v[1] } END { print value + 0 }' "$STATSFILE")
wait_partial_bytes() {
	local current
	if [ ! -f "$STATSFILE" ]; then
		echo 0
		return
	fi
	current=$(awk -F'bytes.received=' '/origin=imtcp/ && /bytes.received=/ { split($2, v, /[^0-9]/); value=v[1] } END { print value + 0 }' "$STATSFILE")
	if [ "$current" -gt "$partial_bytes_before" ]; then
		echo 1
	else
		echo 0
	fi
}
printf '<167>Mar 10 01:00:00 host app: acl-rs-partial' >&9 || error_exit 1
wait_file_lines --count-function wait_partial_bytes "$STATSFILE" 1

# Removing localhost must close the established session, not install a
# callback that leaves its sole maxSessions slot occupied.
sed 's/allowedSender=\["127\.0\.0\.1\/32","192\.0\.2\.1\/32"\]/allowedSender=["192.0.2.1\/32"]/' \
	"$CONF_FILE" >"$CONF_FILE.revoked"
cp "$CONF_FILE.revoked" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript ACL revocation did not activate live: $reload_status"
	error_exit 1
fi
assert_content_missing 'acl-rs-partial'

# Reallow and connect again.  This succeeds only when the revoked session slot
# and its persistent epoll descriptor were reclaimed before fence release.
cp "$CONF_FILE.allowed" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=4"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: RainerScript ACL reallow did not activate live: $reload_status"
	error_exit 1
fi
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: acl-rs-after\n' >&9 || error_exit 1
wait_content 'acl-rs-after' "$RSYSLOG_OUT_LOG"
exec 9>&-
shutdown_when_empty
wait_shutdown
# Drain the action queue before the final negative oracle, so a buggy regular
# close cannot hide a flushed partial frame behind asynchronous output.
assert_content_missing 'acl-rs-partial'
exit_test
