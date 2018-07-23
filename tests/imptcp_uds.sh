#!/bin/bash
echo ======================================================================
echo \[imptcp_uds.sh\]: test imptcp unix domain socket
. $srcdir/diag.sh init
startup imptcp_uds.conf

LONGLINE="$(printf 'A%.0s' {1..124000})"
echo "localhost test: $LONGLINE" | nc -U "$srcdir/testbench_socket"

# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100

PERMS="$(stat -c "%#a" "$srcdir/testbench_socket")"
if [ "$PERMS" != "0600" ]; then
  echo "imptcp_uds.sh failed, incorrect permissions on socket file: $PERMS"
  error_exit
fi

PERMS="$(stat -c "%#a" "$srcdir/testbench_socket2")"
if [ "$PERMS" != "0666" ]; then
  echo "imptcp_uds.sh failed, incorrect permissions on 2nd socket file: $PERMS"
  error_exit
fi

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
LEN="$(cat "./rsyslog.out.log" | wc -c)"
if [ "$LEN" -lt "124000" ]; then
  echo "imptcp_uds.sh failed, should have logged at least 124k characters in a single line"
  error_exit
fi;

exit_test
