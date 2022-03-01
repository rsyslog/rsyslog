#!/bin/bash
# part of the rsyslog project, released under ASL 2.0

. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=50 # enough to generate big file
export MAX_ERROR_SIZE=100
export SEQ_CHECK_OPTIONS=-i2
check_sql_data_ready() {
	mysql_get_data
	seq_check --check-only 0 $((NUMMESSAGES - 2))
}
export QUEUE_EMPTY_CHECK_FUNC=check_sql_data_ready

generate_conf
add_conf '
$ModLoad ../plugins/ommysql/.libs/ommysql
global(errormessagestostderr.maxnumber="5")

template(type="string" name="tpl" string="insert into SystemEvents (Message, Facility) values (\"%msg%\", %$!facility%)" option.sql="on")

if((not($msg contains "error")) and ($msg contains "msgnum:")) then {
	set $.num = field($msg, 58, 2);
	if $.num % 2 == 0 then {
		set $!facility = $syslogfacility;
	} else {
		set $/cntr = 0;
	}
	action(type="ommysql" name="mysql_action" server="127.0.0.1" template="tpl"
	       db="'$RSYSLOG_DYNNAME'" uid="rsyslog" pwd="testbench" action.errorfile="'$RSYSLOG2_OUT_LOG'" action.errorfile.maxsize="'$MAX_ERROR_SIZE'")
}
'
mysql_prep_for_test
startup
injectmsg
shutdown_when_empty
wait_shutdown
mysql_get_data
seq_check  0 $((NUMMESSAGES - 2)) -i2
check_file_exists ${RSYSLOG2_OUT_LOG}
file_size_check ${RSYSLOG2_OUT_LOG} ${MAX_ERROR_SIZE}
exit_test
