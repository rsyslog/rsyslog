#!/bin/bash
echo ======================================================================
echo \[imptcp_uds.sh\]: test imptcp unix domain socket already exists

# First make sure we don't unlink if not asked to
rm -f "$srcdir/testbench_socket"
echo "nope" > "$srcdir/testbench_socket"

. $srcdir/diag.sh init
. $srcdir/diag.sh startup imptcp_uds.conf
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
. $srcdir/diag.sh content-check "imptcp: error while binding unix socket: Address already in use"
. $srcdir/diag.sh exit

# Now make sure we unlink if asked to
echo "ok" > "$srcdir/testbench_socket"

. $srcdir/diag.sh init
. $srcdir/diag.sh startup imptcp_uds_unlink.conf

logger -Tu "$srcdir/testbench_socket" "hello from imptcp uds"

. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
. $srcdir/diag.sh content-check "hello from imptcp uds"
. $srcdir/diag.sh exit
