#!/bin/bash
# addd 2017-03-01 by RGerhards, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$MaxMessageSize 128
global(processInternalMessages="on")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)

'
startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
tcpflood -m5000 -F 65 -M "\"<120> 2011-03-01T11:22:12Z host tag: this is a way too long message that has to be truncatedtest1 test2 test3 test4 test5 abcdefghijklmn test8 test9 test10 test11 test12 test13 test14 test15\""
shutdown_when_empty
wait_shutdown

grep "byte larger than max msg size; message will be split"  $RSYSLOG_OUT_LOG > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected error message from imptcp truncation not found.  $RSYSLOG_OUT_LOG is:"
        cat $RSYSLOG_OUT_LOG
        error_exit 1
fi

exit_test
