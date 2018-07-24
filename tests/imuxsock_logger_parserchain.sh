#!/bin/bash
# Copyright (C) 2015-03-04 by rainer gerhards, released under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ======================================================================
echo \[imuxsock_logger_parserchain.sh\]: test imuxsock

uname
if [ `uname` = "SunOS" ] ; then
   echo "Solaris: FIX ME LOGGER"
   exit 77
fi

. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(	type="imuxsock" socket="testbench_socket"
	useSpecialParser="off"
	parseHostname="on")

template(name="outfmt" type="string" string="%msg:%\n")
*.notice	./rsyslog.out.log;outfmt
'
startup
logger -d --rfc3164 -u testbench_socket test
if [ ! $? -eq 0 ]; then
logger -d -u testbench_socket test
fi;
# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100
shutdown_when_empty
wait_shutdown
cmp rsyslog.out.log $srcdir/resultdata/imuxsock_logger.log
if [ ! $? -eq 0 ]; then
  echo "imuxsock_logger_parserchain.sh failed"
  echo contents of rsyslog.out.log:
  echo \"`cat rsyslog.out.log`\"
  exit 1
fi;
exit_test
