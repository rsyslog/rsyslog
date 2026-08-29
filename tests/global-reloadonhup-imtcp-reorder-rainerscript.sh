#!/bin/bash
# Verify RainerScript endpoint reconciliation pairs named dynamic imtcp
# listeners by stable config identity rather than source order. The first HUP
# swaps both input definitions while moving an explicit default between them;
# effective profiles are unchanged and must publish as REUSE. A subsequent
# live change proves prepare resolves the reordered source entry back to the
# correct runtime listener. Persistent connections and visible output are the
# session-retention oracle; synchronization uses imdiag acknowledgements.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf 'global(config.reloadOnHUP="on")'
add_conf 'module(load="../plugins/imtcp/.libs/imtcp")'
add_conf 'input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" name="first" ruleset="main")'
add_conf 'input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port2" name="second" ruleset="main" flowControl="on")'
add_conf 'ruleset(name="main") {'
add_conf '  action(type="omfile" name="sink" file="'$RSYSLOG_OUT_LOG'")'
add_conf '}'
startup
assign_tcpflood_port2 "$RSYSLOG_DYNNAME.tcpflood_port2"
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
exec 8<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT2"
cp "$CONF_FILE" "$CONF_FILE.base"

# Swap endpoint names and port-file values across source positions. The
# explicit flow-control default remains at the second position, so the raw
# nodes change while name-matched effective profiles remain equivalent.
sed -e "s|$RSYSLOG_DYNNAME.tcpflood_port2|$RSYSLOG_DYNNAME.tcpflood_port.swap|" \
	-e "s|$RSYSLOG_DYNNAME.tcpflood_port\"|$RSYSLOG_DYNNAME.tcpflood_port2\"|" \
	-e "s|$RSYSLOG_DYNNAME.tcpflood_port.swap|$RSYSLOG_DYNNAME.tcpflood_port|" \
	-e 's/name="first"/name="reload-swap"/' \
	-e 's/name="second"/name="first"/' \
	-e 's/name="reload-swap"/name="second"/' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2 unchanged=7 added=0 removed=0 modified=2 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: reordered named endpoints were not reconciled as reuse: $reload_status"
	error_exit 1
fi
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=2 unchanged=9 added=0 removed=0 modified=0 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: reordered endpoint baseline was not retained: $reload_status"
	error_exit 1
fi

# The first runtime endpoint is now the second source object. Changing its
# effective flow-control value must still find and fence it by config name.
sed 's/flowControl="on"/flowControl="off"/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3 unchanged=8 added=0 removed=0 modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: reordered runtime endpoint was not resolved for live activation: $reload_status"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: reorder-first\n' >&9; then error_exit 1; fi
if ! printf '<167>Mar 10 01:00:00 host app: reorder-second\n' >&8; then error_exit 1; fi
wait_content 'reorder-first' "$RSYSLOG_OUT_LOG"
wait_content 'reorder-second' "$RSYSLOG_OUT_LOG"

# A stable config name is not a socket identity. Changing the endpoint tuple
# under that name must remain restart-required and leave generation three and
# both established sessions intact.
sed "s|$RSYSLOG_DYNNAME.tcpflood_port\"|$RSYSLOG_DYNNAME.tcpflood_port.changed\"|" "$CONF_FILE" \
	>"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=3 unchanged=8 added=0 removed=0 modified=1 invalid=0 source_capability=restart_required"* ]]; then
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
