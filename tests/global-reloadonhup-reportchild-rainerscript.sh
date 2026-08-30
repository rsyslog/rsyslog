#!/bin/bash
# Verify that RainerScript can transactionally publish the scalar
# reportChildProcessExits base policy without replacing rules, actions, or the
# omprog runtime. The helper confirms one record and then exits successfully;
# the next direct action observes that exit. Queue/output synchronization proves
# errors suppresses status 0, all reports it, and none suppresses later reports.
# A final two-base-setting candidate proves the one-scalar gate remains atomic.
. ${srcdir:=.}/diag.sh init
require_plugin omprog

helper="$RSYSLOG_DYNNAME.reportchild-helper.sh"
cat >"$helper" <<'HELPER_EOF'
#!/bin/sh
printf 'OK\n'
if IFS= read -r line; then
	printf 'OK\n'
fi
HELPER_EOF
chmod +x "$helper"

generate_conf
add_conf '
global(processInternalMessages="on" config.reloadOnHUP="on" reportChildProcessExits="errors")
module(load="../plugins/omprog/.libs/omprog")
template(name="reportchild_fmt" type="string" string="%msg%\n")

if $msg contains "msgnum:" then {
    action(type="omprog" name="reportchild_program"
           binary="'"$(pwd)/$helper"'" template="reportchild_fmt"
           confirmMessages="on" signalOnClose="off" queue.type="Direct")
}
action(type="omfile" name="reportchild_sink" file="'$RSYSLOG_OUT_LOG'")
'
startup

injectmsg 0 2
wait_content 'msgnum:00000001' "$RSYSLOG_OUT_LOG"
wait_queueempty
custom_assert_content_missing 'exited with status 0' "$RSYSLOG_OUT_LOG"

sed 's/reportChildProcessExits="errors"/reportChildProcessExits="all"/' "$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: reportChildProcessExits=all did not activate: $reload_status"
	error_exit 1
fi
injectmsg 2 2
wait_content 'msgnum:00000003' "$RSYSLOG_OUT_LOG"
wait_content 'exited with status 0' "$RSYSLOG_OUT_LOG"
report_count=$(awk '/exited with status 0/ { count++ } END { print count + 0 }' "$RSYSLOG_OUT_LOG")

sed 's/reportChildProcessExits="all"/reportChildProcessExits="none"/' "$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: reportChildProcessExits=none did not activate: $reload_status"
	error_exit 1
fi
injectmsg 4 2
wait_content 'msgnum:00000005' "$RSYSLOG_OUT_LOG"
wait_queueempty
final_report_count=$(awk '/exited with status 0/ { count++ } END { print count + 0 }' "$RSYSLOG_OUT_LOG")
if [[ "$final_report_count" -ne "$report_count" ]]; then
	echo "FAIL: reportChildProcessExits=none emitted a new successful-child diagnostic"
	error_exit 1
fi

# Base publication deliberately authorizes one supported scalar per generation.
# Changing the reload mode and child policy together must preserve generation 3
# and the active on/none pair.
sed -e 's/config.reloadOnHUP="on"/config.reloadOnHUP="validate"/' \
	-e 's/reportChildProcessExits="none"/reportChildProcessExits="all"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=3"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: combined base-setting change was not rejected atomically: $reload_status"
	error_exit 1
fi

shutdown_when_empty
wait_shutdown
exit_test
