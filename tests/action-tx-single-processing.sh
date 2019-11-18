#!/bin/bash
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=5000
generate_conf
add_conf '
$ModLoad ../plugins/ommysql/.libs/ommysql
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
action(type="omfile" file=`echo $RSYSLOG2_OUT_LOG`)
'
mysql_prep_for_test
startup
injectmsg
shutdown_when_empty
wait_shutdown
mysql_get_data
export SEQ_CHECK_OPTIONS=-i2
seq_check
exit_test
