#!/bin/bash
# test many concurrent tcp connections
echo \[manytcp.sh\]: test concurrent tcp connections

uname
if [ `uname` = "SunOS" ] ; then
   echo "Solaris: FIX ME"
   exit 77
fi

. $srcdir/diag.sh init
. $srcdir/diag.sh startup manytcp.conf
# the config file specifies exactly 1100 connections
. $srcdir/diag.sh tcpflood -c-1100 -m40000
# the sleep below is needed to prevent too-early termination of the tcp listener
sleep 1
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
. $srcdir/diag.sh seq-check 0 39999
. $srcdir/diag.sh exit
