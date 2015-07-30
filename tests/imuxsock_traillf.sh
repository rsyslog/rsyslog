#!/bin/bash
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imuxsock_traillf.conf
# send a message with trailing LF
./syslog_caller -fsyslog_inject-l -m1 -C "uxsock:testbench_socket"
# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
cmp rsyslog.out.log $srcdir/resultdata/imuxsock_traillf.log
if [ ! $? -eq 0 ]; then
  echo "imuxsock_traillf_root.sh failed"
  exit 1
fi;
. $srcdir/diag.sh exit
