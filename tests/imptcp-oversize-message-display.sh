#!/bin/bash
# addd 2017-03-01 by RGerhards, released under ASL 2.0

. $srcdir/diag.sh init
generate_conf
add_conf '
$MaxMessageSize 128
global(processInternalMessages="on" oversizemsg.input.mode="accept")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514")

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)

'
startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<120> 2011-03-01T11:22:12Z host tag: this is a way too long message that has to be truncatedtest1 test2 test3 test4 test5 abcdefghijklmn test8 test9 test10 test11 test12 test13 test14 test15 kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk tag: testtestetstetstetstetstetsstetstetsytetestetste\""
shutdown_when_empty
wait_shutdown

grep "imptcp: message received.*150 byte larger.*will be split.*\"ghijkl" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected error message from imptcp truncation not found. rsyslog.out.log is:"
        cat rsyslog.out.log
        error_exit 1
fi

grep "imptcp: message received.*22 byte larger.*will be split.*\"sstets" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected error message from imptcp truncation not found. rsyslog.out.log is:"
        cat rsyslog.out.log
        error_exit 1
fi

exit_test
