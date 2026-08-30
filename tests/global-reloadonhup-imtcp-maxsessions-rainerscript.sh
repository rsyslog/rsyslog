#!/bin/bash
# Verify allocation-free imtcp maxSessions growth under the reload fence.
# A held session fills the initial one-slot table and rsyslog's own rejection
# diagnostic proves the cap before HUP. Module- and input-level growth then
# admit one additional persistent session each without disconnecting older
# peers. A shrink and a growth across the next implicit-backlog bucket are
# rejected, and all three established sessions remain usable. Values one
# through three share one bucket, proving the source-default path without
# changing the active socket backlog.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf 'global(config.reloadOnHUP="on")'
add_conf 'module(load="../plugins/imtcp/.libs/imtcp" maxSessions="1")'
add_conf 'input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" name="sessions" ruleset="main")'
add_conf 'ruleset(name="main") {'
add_conf '  action(type="omfile" name="sink" file="'$RSYSLOG_OUT_LOG'")'
add_conf '}'
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"

# The second handshake may complete from the kernel backlog, but SessAccept
# must reject it because the sole userspace slot is occupied.
exec 8<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: rejected-before-grow\n' >&8 || error_exit 1
wait_content 'too many tcp sessions - dropping incoming request' "${RSYSLOG_DYNNAME}.started"
exec 8>&-

cp "$CONF_FILE" "$CONF_FILE.startup"
sed 's/maxSessions="1"/maxSessions="2"/' "$CONF_FILE.startup" >"$CONF_FILE.module-grow"
cp "$CONF_FILE.module-grow" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: module maxSessions growth did not activate: $reload_status"
	error_exit 1
fi
exec 8<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: module-grow-old\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: module-grow-new\n' >&8 || error_exit 1
wait_content 'module-grow-old' "$RSYSLOG_OUT_LOG"
wait_content 'module-grow-new' "$RSYSLOG_OUT_LOG"

sed 's/ruleset="main")/maxSessions="3" ruleset="main")/' "$CONF_FILE.module-grow" >"$CONF_FILE.input-grow"
cp "$CONF_FILE.input-grow" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"modified=1 invalid=0"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: input maxSessions growth did not activate: $reload_status"
	error_exit 1
fi
exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: input-grow-first\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: input-grow-second\n' >&8 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: input-grow-third\n' >&7 || error_exit 1
wait_content 'input-grow-first' "$RSYSLOG_OUT_LOG"
wait_content 'input-grow-second' "$RSYSLOG_OUT_LOG"
wait_content 'input-grow-third' "$RSYSLOG_OUT_LOG"

sed 's/maxSessions="3"/maxSessions="2"/' "$CONF_FILE.input-grow" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=3"* ||
      "$reload_status" != *"source_capability=restart_required"* ]]; then
	echo "FAIL: maxSessions shrink was not rejected: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: shrink-kept-first\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: shrink-kept-second\n' >&8 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: shrink-kept-third\n' >&7 || error_exit 1
wait_content 'shrink-kept-first' "$RSYSLOG_OUT_LOG"
wait_content 'shrink-kept-second' "$RSYSLOG_OUT_LOG"
wait_content 'shrink-kept-third' "$RSYSLOG_OUT_LOG"

sed 's/maxSessions="3"/maxSessions="10"/' "$CONF_FILE.input-grow" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=3"* ||
      "$reload_status" != *"source_capability=restart_required"* ]]; then
	echo "FAIL: implicit-backlog bucket change was not rejected: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: backlog-kept-first\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: backlog-kept-second\n' >&8 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: backlog-kept-third\n' >&7 || error_exit 1
wait_content 'backlog-kept-first' "$RSYSLOG_OUT_LOG"
wait_content 'backlog-kept-second' "$RSYSLOG_OUT_LOG"
wait_content 'backlog-kept-third' "$RSYSLOG_OUT_LOG"

exec 7>&-
exec 8>&-
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
