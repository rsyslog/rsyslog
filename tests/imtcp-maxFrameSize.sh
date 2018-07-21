#!/bin/bash
# addd 2016-05-13 by RGerhards, released under ASL 2.0

. $srcdir/diag.sh init
generate_conf
add_conf '
global(processInternalMessages="on")
module(load="../plugins/imtcp/.libs/imtcp" maxFrameSize="100")
input(type="imtcp" port="13514")

action(type="omfile" file="rsyslog.out.log")
'
startup
. $srcdir/diag.sh tcpflood -m1 -M "\"1005 <120> 2011-03-01T11:22:12Z host tag: this is a way too long message\""
shutdown_when_empty
wait_shutdown

grep "Framing Error.*change to octet stuffing" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected error message from imtcp not found. rsyslog.out.log is:"
        cat rsyslog.out.log
        error_exit 1
fi


exit_test
