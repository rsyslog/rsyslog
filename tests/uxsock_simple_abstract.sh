#!/bin/bash
# This tests basic omuxsock functionality. A socket receiver is started which sends
# all data to an output file, then a rsyslog instance is started which generates
# messages and sends them to the unix socket. Datagram sockets are being used.
# added 2010-08-06 by Rgerhards
# Updated 2025-04-16 for abstract sockets
. ${srcdir:=.}/diag.sh init
check_command_available timeout

uname
if [ $(uname) != "Linux" ] ; then
    echo "This test requires Linux (AF_UNIX abstract addresses)"
    exit 77
fi

SOCKET_NAME="$RSYSLOG_DYNNAME-testbench-dgram-uxsock-abstract"

# create the pipe and start a background process that copies data from
# it to the "regular" work file
generate_conf
add_conf '
$MainMsgQueueTimeoutShutdown 10000

$template outfmt,"%msg:F,58:2%\n"

module(
    load = "../plugins/omuxsock/.libs/omuxsock"
    template = "outfmt"
)

:msg, contains, "msgnum:" action(
    type = "omuxsock"
    SocketName = "'$SOCKET_NAME'"
    abstract = "1"
)
'
timeout 30s ./uxsockrcvr -a -s$SOCKET_NAME -o $RSYSLOG_OUT_LOG -t 60 &
BGPROCESS=$!
echo background uxsockrcvr process id is $BGPROCESS

# now do the usual run
startup
# 10000 messages should be enough
injectmsg 0 10000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown

# wait for the cp process to finish, do pipe-specific cleanup
echo shutting down uxsockrcvr...
# TODO: we should do this more reliable in the long run! (message counter? timeout?)
kill $BGPROCESS
wait $BGPROCESS
echo background process has terminated, continue test...

# and continue the usual checks
seq_check 0 9999
exit_test
