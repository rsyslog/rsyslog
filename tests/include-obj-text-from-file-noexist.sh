#!/bin/bash
# added 2018-01-22 by Rainer Gerhards; Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
if $msg contains "msgnum:" then {
	include(text=`cat '${srcdir}'/testsuites/DOES-NOT-EXIST`)
}
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_when_empty
wait_shutdown
content_check --regex 'file could not be accessed for `cat .*/testsuites/DOES-NOT-EXIST'
exit_test
