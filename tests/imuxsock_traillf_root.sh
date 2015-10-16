#!/bin/bash
# note: we must be root and no other syslogd running in order to
# carry out this test
echo \[imuxsock_traillf_root.sh\]: test trailing LF handling in imuxsock
echo This test must be run as root with no other active syslogd
if [ "$EUID" -ne 0 ]; then
    exit 77 # Not root, skip this test
fi
./syslog_caller -fsyslog_inject-l -m0 > /dev/null 2>&1
no_liblogging_stdlog=$?
if [ $no_liblogging_stdlog -ne 0 ];then
  echo "liblogging-stdlog not available - skipping test"
  exit 77
fi
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imuxsock_traillf_root.conf
# send a message with trailing LF
./syslog_caller -fsyslog_inject-l -m1
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
