#!/bin/bash
echo ======================================================================
echo \[imptcp_uds.sh\]: test imptcp unix domain socket
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imptcp_uds.conf

LONGLINE="$(printf 'A%.0s' {1..124000})"
echo "localhost test: $LONGLINE" | nc -U "$srcdir/testbench_socket"

# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100

PERMS="$(stat -c "%#a" "$srcdir/testbench_socket")"
if [ "$PERMS" != "0600" ]; then
  echo "imptcp_uds.sh failed, incorrect permissions on socket file: $PERMS"
  . $srcdir/diag.sh error-exit
fi

PERMS="$(stat -c "%#a" "$srcdir/testbench_socket2")"
if [ "$PERMS" != "0666" ]; then
  echo "imptcp_uds.sh failed, incorrect permissions on 2nd socket file: $PERMS"
  . $srcdir/diag.sh error-exit
fi

. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
LEN="$(cat "./rsyslog.out.log" | wc -c)"
if [ "$LEN" -lt "124000" ]; then
  echo "imptcp_uds.sh failed, should have logged at least 124k characters in a single line"
  . $srcdir/diag.sh error-exit
fi;

. $srcdir/diag.sh exit
