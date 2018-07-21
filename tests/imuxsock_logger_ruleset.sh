#!/bin/bash
# rgerhards, 2016-02-02 released under ASL 2.0
echo \[imuxsock_logger_ruleset.sh\]: test imuxsock with ruleset definition
. $srcdir/diag.sh init
startup imuxsock_logger_ruleset.conf
# send a message with trailing LF
logger -d -u testbench_socket test
# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
cmp rsyslog.out.log $srcdir/resultdata/imuxsock_logger.log
  echo \"`cat rsyslog.out.log`\"
if [ ! $? -eq 0 ]; then
  echo "imuxsock_logger.sh failed"
  echo contents of rsyslog.out.log:
  echo \"`cat rsyslog.out.log`\"
  exit 1
fi;
exit_test
