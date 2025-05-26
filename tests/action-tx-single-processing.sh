#!/bin/bash
# part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_TSAN	# for some reason, this test is extremely slow under tsan, causing timeout fail
export NUMMESSAGES=2000
export SEQ_CHECK_OPTIONS=-i2
check_sql_data_ready() {
	mysql_get_data
	seq_check --check-only 0 $((NUMMESSAGES - 2))
}
export QUEUE_EMPTY_CHECK_FUNC=check_sql_data_ready
generate_conf
add_conf '
module(load="../plugins/ommysql/.libs/ommysql")
global(errormessagestostderr.maxnumber="50")

template(type="string" name="tpl" string="insert into SystemEvents (Message, Facility) values (\"%msg%\", %$!facility%)" option.sql="on")
template(type="string" name="tpl2" string="%$.num%|%$!facility%|insert into SystemEvents (Message, Facility) values (\"%msg%\", %$!facility%)\n" option.sql="on")

if($msg contains "msgnum:") then {
	set $.num = field($msg, 58, 2);
	if $.num % 2 == 0 then {
		set $!facility = $syslogfacility;
	} else {
		set $/cntr = 0;
	}
	action(type="ommysql" name="mysql_action" server="127.0.0.1" template="tpl"
	       db="'$RSYSLOG_DYNNAME'" uid="rsyslog" pwd="testbench")
}
action(type="omfile" file="'$RSYSLOG2_OUT_LOG'")
'
mysql_prep_for_test
startup
injectmsg
shutdown_when_empty
wait_shutdown
mysql_get_data
seq_check 0 $((NUMMESSAGES - 2))
exit_test
