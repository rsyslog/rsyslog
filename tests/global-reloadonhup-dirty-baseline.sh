#!/bin/bash
# A daemon may historically continue when abortOnUncleanConfig is disabled,
# but that partially applied startup must never become a reload baseline. The
# deterministic HUP acknowledgement plus getreloadstatus prove a same-file
# candidate is rejected as baseline_unavailable; the injected record proves
# the active, usable portion of the startup configuration remains running.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(abortOnUncleanConfig="off" config.reloadOnHUP="validate" reloadUnknownParameter="invalid")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=baseline_unavailable active_generation=1"* ]]; then
	echo "FAIL: dirty startup was exposed as a reload baseline: $reload_status"
	error_exit 1
fi
injectmsg 0 1
wait_queueempty
shutdown_when_empty
wait_shutdown
content_check 'msgnum:00000000'

exit_test
