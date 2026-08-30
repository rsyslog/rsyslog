#!/bin/bash
# Verify transactional replacement of a named imtcp endpoint with a different
# fixed socket tuple. A seed listener lets the kernel select the replacement
# port without an external free-port race; it is retired before prepare binds
# that port. HUP completion proves publication, failed connects prove each old
# accept socket is closed, and records on the established old session plus the
# new listener prove drain-retirement without loss. The bounded retirement
# loop observes state, not elapsed time, and exists only as a hang guard.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf 'global(config.reloadOnHUP="on")'
add_conf 'module(load="../plugins/imtcp/.libs/imtcp")'
add_conf 'input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" name="replace" ruleset="main")'
add_conf 'input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port2" name="seed" ruleset="main")'
add_conf 'ruleset(name="main") {'
add_conf '  action(type="omfile" name="sink" file="'$RSYSLOG_OUT_LOG'")'
add_conf '}'
startup
assign_tcpflood_port2 "$RSYSLOG_DYNNAME.tcpflood_port2"
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
REPLACEMENT_PORT="$TCPFLOOD_PORT2"
cp "$CONF_FILE" "$CONF_FILE.startup"

# Release the kernel-selected replacement port before private prepare binds it.
sed 's/input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port2" name="seed" ruleset="main")//' \
	"$CONF_FILE.startup" >"$CONF_FILE.base"
cp "$CONF_FILE.base" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"removed=1"* ||
      "$reload_status" != *"source_capability=drain_replace"* ||
      "$reload_status" != *"retirement_pending=0"* ]]; then
	echo "FAIL: replacement seed did not retire: $reload_status"
	error_exit 1
fi
if (exec 7<>"/dev/tcp/127.0.0.1/$REPLACEMENT_PORT") 2>/dev/null; then
	echo "FAIL: retired seed still accepts replacement-port connections"
	error_exit 1
fi

# The normalized input identity remains name=replace, while the effective
# endpoint changes from dynamic/portfile to a fixed tuple. Prepare must create
# the fixed listener before commit and retire only the old accept side.
sed 's|port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" name="replace"|address="127.0.0.1" port="'$REPLACEMENT_PORT'" name="replace"|' \
	"$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"modified=1 invalid=0"* ||
      "$reload_status" != *"source_capability=drain_replace"* ||
      "$reload_status" != *"retirement_pending=1"* ]]; then
	echo "FAIL: fixed endpoint replacement was not activated: $reload_status"
	error_exit 1
fi
if (exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT") 2>/dev/null; then
	echo "FAIL: replaced listener still accepts new sessions"
	error_exit 1
fi
exec 8<>"/dev/tcp/127.0.0.1/$REPLACEMENT_PORT"
if ! printf '<167>Mar 10 01:00:00 host app: replace-old-session\n' >&9; then error_exit 1; fi
if ! printf '<167>Mar 10 01:00:00 host app: replace-new-listener\n' >&8; then error_exit 1; fi
wait_content 'replace-old-session' "$RSYSLOG_OUT_LOG"
wait_content 'replace-new-listener' "$RSYSLOG_OUT_LOG"

exec 9>&-
for ((retire_try = 1; retire_try <= 50; ++retire_try)); do
	reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
	if [[ "$reload_status" == *"result=activated active_generation=3"* &&
	      "$reload_status" == *"retirement_pending=0"* ]]; then break; fi
	"$TESTTOOL_DIR/msleep" 100
done
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"retirement_pending=0"* ]]; then
	echo "FAIL: replaced endpoint did not retire after its session closed: $reload_status"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: replace-after-retire\n' >&8; then error_exit 1; fi
wait_content 'replace-after-retire' "$RSYSLOG_OUT_LOG"

exec 8>&-
shutdown_when_empty
wait_shutdown
exit_test
