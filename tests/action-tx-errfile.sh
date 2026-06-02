#!/bin/bash
# added by Rainer Gerhards 2018-01-05
# part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=50 # sufficient for our needs!
export SEQ_CHECK_OPTIONS=-i2
EXPECTED_ERRFILE="$(cat "${srcdir}/testsuites/action-tx-errfile.result")" || exit 1
export EXPECTED_ERRFILE
EXPECTED_ERRFILE_LINES="$(printf '%s\n' "$EXPECTED_ERRFILE" | wc -l)"
export EXPECTED_ERRFILE_LINES
check_sql_and_errfile_data_ready() {
	mysql_get_data
	seq_check --check-only 0 $((NUMMESSAGES - 2)) || return 1

	if [ ! -f "$RSYSLOG2_OUT_LOG" ]; then
		return 1
	fi
	current_errfile_lines=$(wc -l < "$RSYSLOG2_OUT_LOG")
	if [ "$current_errfile_lines" -ne "$EXPECTED_ERRFILE_LINES" ]; then
		return 1
	fi
	printf '%s\n' "$EXPECTED_ERRFILE" | cmp - "$RSYSLOG2_OUT_LOG" > /dev/null
}
export QUEUE_EMPTY_CHECK_FUNC=check_sql_and_errfile_data_ready

generate_conf
printf '%s\n' \
	'INFO: this test intentionally generates MySQL syntax errors for odd messages.' \
	'INFO: odd messages leave $!facility unset, causing ommysql RS_RET_DATAFAIL -2218.' \
	'INFO: the expected SQL errors must be recorded in action.errorfile.'
add_conf '
$ModLoad ../plugins/ommysql/.libs/ommysql
global(errormessagestostderr.maxnumber="5")

template(type="string" name="tpl" string="insert into SystemEvents (Message, Facility) values (\"%msg%\", %$!facility%)" option.sql="on")

if((not($msg contains "error")) and ($msg contains "msgnum:")) then {
	set $.num = field($msg, 58, 2);
	if $.num % 2 == 0 then {
		set $!facility = $syslogfacility;
	} else {
		# Intentionally leave $!facility unset so this odd message generates
		# a MySQL syntax error. The test verifies that the failed action is
		# written to action.errorfile, not that SQL insertion succeeds.
		set $/cntr = 0;
	}
	action(type="ommysql" name="mysql_action" server="127.0.0.1" template="tpl"
	       db="'$RSYSLOG_DYNNAME'" uid="rsyslog" pwd="testbench" action.errorfile="'$RSYSLOG2_OUT_LOG'")
}
'
mysql_prep_for_test
startup
injectmsg
shutdown_when_empty
wait_shutdown
export EXPECTED="$EXPECTED_ERRFILE"
cmp_exact ${RSYSLOG2_OUT_LOG}
mysql_get_data
seq_check  0 $((NUMMESSAGES - 2)) -i2
exit_test
