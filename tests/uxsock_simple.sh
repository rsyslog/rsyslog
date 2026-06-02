#!/bin/bash
# This tests basic omuxsock functionality. A socket receiver is started which sends
# all data to an output file, then a rsyslog instance is started which generates
# messages and sends them to the unix socket. Datagram sockets are being used.
# added 2010-08-06 by Rgerhards
. ${srcdir:=.}/diag.sh init
uname
if [ $(uname) = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

wait_receiver_ready() {
    expected_generation=$1
    ready_file=$2
    count=1

    while [ "$count" -le 100 ]; do
        if [ -r "$ready_file" ] && [ "$(cat "$ready_file")" = "$expected_generation" ]; then
            return
        fi
        $TESTTOOL_DIR/msleep 100
        count=$((count + 1))
    done
    echo "uxsockrcvr did not report ready generation $expected_generation for $ready_file"
    error_exit 1
}

# create the pipe and start a background process that copies data from
# it to the "regular" work file
generate_conf
add_conf '
$MainMsgQueueTimeoutShutdown 10000

$ModLoad ../plugins/omuxsock/.libs/omuxsock
$template outfmt,"%msg:F,58:2%\n"
$OMUXSockSocket '$RSYSLOG_DYNNAME'-testbench-dgram-uxsock
:msg, contains, "msgnum:" :omuxsock:;outfmt
'
RECEIVER_READY=$RSYSLOG_DYNNAME.uxsockrcvr.ready
./uxsockrcvr -s$RSYSLOG_DYNNAME-testbench-dgram-uxsock -o $RSYSLOG_OUT_LOG -r $RECEIVER_READY -t 60 &
BGPROCESS=$!
echo background uxsockrcvr process id is $BGPROCESS
wait_receiver_ready 1 "$RECEIVER_READY"

# now do the usual run
startup
# 10000 messages should be enough
injectmsg 0 10000
wait_queueempty
echo resetting uxsockrcvr...
kill -HUP $BGPROCESS
wait_receiver_ready 2 "$RECEIVER_READY"
injectmsg 10000 10000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown

# wait for the cp process to finish, do pipe-specific cleanup
echo shutting down uxsockrcvr...
# TODO: we should do this more reliable in the long run! (message counter? timeout?)
kill $BGPROCESS
wait $BGPROCESS
echo background process has terminated, continue test...

# and continue the usual checks
seq_check 0 19999
exit_test
