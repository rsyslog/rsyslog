#!/bin/bash
# addd 2018-04-03 by RGerhards, released under ASL 2.0
. $srcdir/diag.sh init
generate_conf
add_conf '
action(type="omfile" file=" ")
action(type="omfile" file="rsyslog.out.log")
'
startup
shutdown_when_empty
wait_shutdown

grep "only of whitespace" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
	echo
	echo "FAIL: expected error message not found. rsyslog.out.log is:"
	cat rsyslog.out.log
	error_exit 1
fi

exit_test
