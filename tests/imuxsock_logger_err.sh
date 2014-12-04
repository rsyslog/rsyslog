# this is primarily a safeguard to ensure the imuxsock tests basically work
# added 2014-12-04 by Rainer Gerhards, licensed under ASL 2.0
echo \[imuxsock_logger.sh\]: test imuxsock
source $srcdir/diag.sh init
source $srcdir/diag.sh startup imuxsock_logger.conf
# send a message with trailing LF
logger -d -u testbench_socket "wrong message"
# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
cmp rsyslog.out.log $srcdir/resultdata/imuxsock_logger.log
if [ $? -eq 0 ]; then
  echo "imuxsock_logger_err.sh did not report an error where it should!"
  exit 1
else
  echo "all well, we saw the error that we wanted to have"
fi;
source $srcdir/diag.sh exit
