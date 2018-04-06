#!/bin/bash
# This tests the border case that a message is exactly as large as the default
# buffer size (101 chars) and is reduced in size afterwards. This has been seen
# in practice.
# see also https://github.com/rsyslog/rsyslog/issues/1658
# Copyright (C) 2017 by Rainer Gerhards, released under ASL 2.0 (2017-07-11)

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi
if [ `uname` = "SunOS" ] ; then
   echo "This test currently does not work on all flavors of Solaris."
   exit 77
fi

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(type="imuxsock" Socket="testbench_socket")

template(name="outfmt" type="string" string="%msg%\n")

ruleset(name="rs" queue.type="LinkedList") {
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
	stop
}

*.notice call rs
'
. $srcdir/diag.sh startup
logger -d -u testbench_socket -t RSYSLOG_TESTBENCH 'test 01234567890123456789012345678901234567890123456789012345
' #Note: LF at end of message is IMPORTANT, it is bug triggering condition
# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
echo " test 01234567890123456789012345678901234567890123456789012345" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "msgdup.sh failed"
  echo contents of rsyslog.out.log:
  echo \"`cat rsyslog.out.log`\"
  exit 1
fi;
. $srcdir/diag.sh exit
