#!/bin/bash
echo \[imuxsock_hostname.sh\]: test set hostname
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imuxsock_hostname.conf
# the message itself is irrelevant. The only important thing is
# there is one
./syslog_caller -m1 -C "uxsock:testbench_socket"
# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
cmp rsyslog.out.log $srcdir/resultdata/imuxsock_hostname.log
if [ ! $? -eq 0 ]; then
  echo "imuxsock_hostname.sh failed"
  exit 1
fi;
. $srcdir/diag.sh exit
