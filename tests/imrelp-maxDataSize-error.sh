#!/bin/bash
# add 2018-04-26 by Pascal Withopf, released under ASL 2.0
echo [imrelp-maxDataSize-error.sh]
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp")

global(
	maxMessageSize="300"
)

input(type="imrelp" port="13514" maxDataSize="250")

action(type="omfile" file="rsyslog.out.log")
'
startup
./msleep 2000

shutdown_when_empty
wait_shutdown

grep "error: maxDataSize.*smaller than global parameter maxMessageSize" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected error message not found. rsyslog.out.log is:"
        cat rsyslog.out.log
        error_exit 1
fi

exit_test
