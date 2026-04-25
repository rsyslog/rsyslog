#!/bin/bash
# This tests basic omuxsock functionality. Multiple socket receivers are started
# which sends all data to an output file, then a rsyslog instance is started which
# generates messages and sends them to multiple unix sockets. Datagram sockets are
# being used.
# Based on uxsock_simple.sh added 2010-08-06 by Rgerhards
# Updated 2025-04-16 for abstract sockets
. ${srcdir:=.}/diag.sh init
uname
if [ $(uname) != "Linux" ] ; then
    echo "This test requires Linux (AF_UNIX abstract addresses)"
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

SOCKET_NAMES="
$RSYSLOG_DYNNAME-testbench-dgram-uxsock.0
$RSYSLOG_DYNNAME-testbench-dgram-uxsock.DGRAM
$RSYSLOG_DYNNAME-testbench-dgram-uxsock.STREAM
$RSYSLOG_DYNNAME-testbench-dgram-uxsock.SEQPACKET
"

# create the pipe and start a background process that copies data from
# it to the "regular" work file
generate_conf
for name in $SOCKET_NAMES; do
    ext=${name##*.}
    if [ ${ext} = "0" ]; then
        add_conf '

        $template outfmt,"%msg:F,58:2%\n"

        module(
            load = "../plugins/omuxsock/.libs/omuxsock"
            template = "outfmt"
            SocketName = "'${name}'"
            abstract = "1"
        )
        :msg, contains, "msgnum:" {
            action(
                type = "omuxsock"
            )
        '
    else
        add_conf '
            action(
                type = "omuxsock"
                SocketName = "'$name'"
                abstract = "1"
                SocketType = "'$ext'"
            )
        '
    fi
done
add_conf '
}
'
BGPROCESS=""
RECEIVER_READY=""
for name in $SOCKET_NAMES; do
    ext=${name##*.}
    if [ "${ext}" = "0" ]; then
        type=""
    else
        type="-T${ext}"
    fi
    ready_file=${RSYSLOG_DYNNAME}.uxsockrcvr.${ext}.ready
    ./uxsockrcvr -a -s$name ${type} -o ${RSYSLOG_OUT_LOG}.${ext} -r ${ready_file} -t 60 &
    PID=$!
    BGPROCESS="$BGPROCESS $PID"
    RECEIVER_READY="$RECEIVER_READY $ready_file"
    echo background uxsockrcvr ${name} process id is $PID
    wait_receiver_ready 1 "$ready_file"
done

# now do the usual run
RS_REDIR="> ${RSYSLOG_DYNNAME}.log 2>&1"
startup
# 10000 messages should be enough
injectmsg 0 10000
wait_queueempty
echo resetting uxsockrcvr...
for pid in $BGPROCESS; do
    kill -HUP $pid
done
for ready_file in $RECEIVER_READY; do
    wait_receiver_ready 2 "$ready_file"
done
injectmsg 10000 1
wait_queueempty
injectmsg 10001 9999
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown

# wait for the cp process to finish, do pipe-specific cleanup
echo shutting down uxsockrcvr...
# TODO: we should do this more reliable in the long run! (message counter? timeout?)
for pid in $BGPROCESS; do
    kill $pid
    wait $pid
done
echo background processes have terminated, continue test...

# and continue the usual checks.
BASE=${RSYSLOG_OUT_LOG}
for name in $SOCKET_NAMES; do
    SEQ_CHECK_FILE=${BASE}.${name##*.}
    case ${name##*.} in
        STREAM|SEQPACKET)
            echo 10000 >> ${SEQ_CHECK_FILE}
            ;;
    esac
    seq_check 0 19999
done

# Verify that we get messages for when our receiver reset
# We don't redirect with valgrind
if [ "${USE_VALGRIND}" != "YES" ]; then
    content_check "omuxsock suspending: send(), socket " ${RSYSLOG_DYNNAME}.log
fi
exit_test
