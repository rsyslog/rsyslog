#!/bin/bash
# test many concurrent tcp connections

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo \[manytcp.sh\]: test concurrent tcp connections

uname
if [ `uname` = "SunOS" ] ; then
   echo "Solaris: FIX ME"
   exit 77
fi

. $srcdir/diag.sh init
startup manytcp.conf
# the config file specifies exactly 1100 connections
. $srcdir/diag.sh tcpflood -c-1100 -m40000
# the sleep below is needed to prevent too-early termination of the tcp listener
sleep 1
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
seq_check 0 39999
exit_test
