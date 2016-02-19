#!/bin/bash
# note: we use the system socket, but assign a different name to
# it. This is not 100% the same thing as running as root, but it
# is pretty close to it. -- rgerhards, 201602-19
echo \[imuxsock_logger_syssock.sh\]: test trailing LF handling in imuxsock
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imuxsock_logger_syssock.conf
# send a message with trailing LF
logger -d -u testbench_socket test
# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
cmp rsyslog.out.log $srcdir/resultdata/imuxsock_logger.log
if [ ! $? -eq 0 ]; then
  echo "imuxsock_logger_syssock.sh failed"
  echo contents of rsyslog.out.log:
  echo \"`cat rsyslog.out.log`\"
  exit 1
fi;
. $srcdir/diag.sh exit
