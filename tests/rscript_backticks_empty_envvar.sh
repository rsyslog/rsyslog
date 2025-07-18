#!/bin/bash
# added 2018-11-02 by rgerhards
# see also https://github.com/rsyslog/rsyslog/issues/3006
# released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
if `echo $DOES_NOT_EXIST` == "" then
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
'
startup
shutdown_when_empty
wait_shutdown
check_exit
content_check --regex "rsyslogd: .*start"
exit_test
