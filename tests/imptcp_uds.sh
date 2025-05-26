#!/bin/bash
echo ======================================================================
echo \[imptcp_uds.sh\]: test imptcp unix domain socket
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(MaxMessageSize="124k")

module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" path="'$RSYSLOG_DYNNAME'-testbench_socket" unlink="on" filecreatemode="0600")
input(type="imptcp" path="'$RSYSLOG_DYNNAME'-testbench_socket2" unlink="on" filecreatemode="0666")

template(name="outfmt" type="string" string="%msg:%\n")
*.notice      action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup

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
LEN="$(wc <${RSYSLOG_OUT_LOG} -c)"
if [ "$LEN" -lt "124000" ]; then
  echo "imptcp_uds.sh failed, should have logged at least 124k characters in a single line"
  error_exit
fi;

exit_test
