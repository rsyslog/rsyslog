#!/bin/bash
# rgerhards, 2016-02-02 released under ASL 2.0
echo \[imuxsock_logger_ruleset.sh\]: test imuxsock with ruleset definition
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imuxsock_logger_ruleset.conf
# send a message with trailing LF
logger -d -u testbench_socket test
# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
cmp rsyslog.out.log $srcdir/resultdata/imuxsock_logger.log
  echo \"`cat rsyslog.out.log`\"
if [ ! $? -eq 0 ]; then
  echo "imuxsock_logger.sh failed"
  echo contents of rsyslog.out.log:
  echo \"`cat rsyslog.out.log`\"
  exit 1
fi;
. $srcdir/diag.sh exit
