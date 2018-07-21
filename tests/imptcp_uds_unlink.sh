#!/bin/bash
echo ======================================================================
echo \[imptcp_uds.sh\]: test imptcp unix domain socket already exists

# First make sure we don't unlink if not asked to
rm -f "$srcdir/testbench_socket"
echo "nope" > "$srcdir/testbench_socket"

. $srcdir/diag.sh init
startup imptcp_uds.conf
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
. $srcdir/diag.sh content-check "imptcp: error while binding unix socket: Address already in use"
exit_test

# Now make sure we unlink if asked to
echo "ok" > "$srcdir/testbench_socket"

. $srcdir/diag.sh init
startup imptcp_uds_unlink.conf

logger -Tu "$srcdir/testbench_socket" "hello from imptcp uds"

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
. $srcdir/diag.sh content-check "hello from imptcp uds"
exit_test
