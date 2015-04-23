echo \[imuxsock_logger.sh\]: test imuxsock
source $srcdir/diag.sh init
source $srcdir/diag.sh startup imuxsock_logger.conf
# send a message with trailing LF
logger -d -u testbench_socket test
# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
cmp rsyslog.out.log $srcdir/resultdata/imuxsock_logger.log
if [ ! $? -eq 0 ]; then
  echo "imuxsock_logger.sh failed"
  echo contents of rsyslog.out.log:
  echo \"`cat rsyslog.out.log`\"
  exit 1
fi;
source $srcdir/diag.sh exit
