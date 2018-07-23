#!/bin/bash
# This test tests tcp forwarding in a network namespace with assigned template.
# To do so, a simple tcp listener service is started in a network namespace.
# Released under GNU GPLv3+
echo ===============================================================================
echo \[tcp_forwarding_ns_tpl.sh\]: test for tcp forwarding in a network namespace with assigned template
echo This test must be run as root [network namespace creation/change required]
if [ "$EUID" -ne 0 ]; then
    exit 77 # Not root, skip this test
fi

# create the pipe and start a background process that copies data from 
# it to the "regular" work file
. $srcdir/diag.sh init

# create network namespace and bring it up
ip netns add rsyslog_test_ns
ip netns exec rsyslog_test_ns ip link set dev lo up

# run server in namespace
ip netns exec rsyslog_test_ns ./minitcpsrv -t127.0.0.1 -p13514 -frsyslog.out.log &
BGPROCESS=$!
echo background minitcpsrvr process id is $BGPROCESS

# now do the usual run
startup tcp_forwarding_ns_tpl.conf
# 10000 messages should be enough
. $srcdir/diag.sh injectmsg 0 10000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown

# note: minitcpsrvr shuts down automatically if the connection is closed!
# (we still leave the code here in in case we need it later)
#echo shutting down minitcpsrv...
#kill $BGPROCESS
#wait $BGPROCESS
#echo background process has terminated, continue test...

# remove network namespace
ip netns delete rsyslog_test_ns

# and continue the usual checks
seq_check 0 9999
exit_test
