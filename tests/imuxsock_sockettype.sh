#!/bin/bash
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(type="imuxsock" Socket="'$RSYSLOG_DYNNAME'-testbench_socket" SocketType="stream")

template(name="outfmt" type="string" string="%msg:%\n")
*.notice      action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
# connect to a stream socket and send a message
logger -u $RSYSLOG_DYNNAME-testbench_socket test1
logger -u $RSYSLOG_DYNNAME-testbench_socket test2
logger -u $RSYSLOG_DYNNAME-testbench_socket test3
# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
cmp $RSYSLOG_OUT_LOG $srcdir/resultdata/imuxsock_sockettype.log
if [ ! $? -eq 0 ]; then
  echo "imuxsock_sockettype.sh failed"
  echo "contents of $RSYSLOG_OUT_LOG:"
  echo \"$(cat $RSYSLOG_OUT_LOG)\"
  error_exit 1
fi;
exit_test
