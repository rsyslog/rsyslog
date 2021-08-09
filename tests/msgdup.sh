#!/bin/bash
# This tests the border case that a message is exactly as large as the default
# buffer size (101 chars) and is reduced in size afterwards. This has been seen
# in practice.
# see also https://github.com/rsyslog/rsyslog/issues/1658
# Copyright (C) 2017 by Rainer Gerhards, released under ASL 2.0 (2017-07-11)
. ${srcdir:=.}/diag.sh init
check_logger_has_option_d
generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(type="imuxsock" Socket="'$RSYSLOG_DYNNAME'-testbench_socket")

template(name="outfmt" type="string" string="%msg%\n")

ruleset(name="rs" queue.type="LinkedList") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
	stop
}

*.=notice call rs
'
startup
logger -d -u $RSYSLOG_DYNNAME-testbench_socket -t RSYSLOG_TESTBENCH 'test 01234567890123456789012345678901234567890123456789012345
' #Note: LF at end of message is IMPORTANT, it is bug triggering condition
# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
export EXPECTED=" test 01234567890123456789012345678901234567890123456789012345"
cmp_exact $RSYSLOG_OUT_LOG
exit_test
