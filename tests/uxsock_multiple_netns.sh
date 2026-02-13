#!/bin/bash
# This tests basic omuxsock functionality with namespaces.
# Multiple socket receivers are started which sends all data
# to an output file, then a rsyslog instance is started which
# generates messages and sends them to multiple unix sockets.
# Multiple socket types are being used.
# Based on uxsock_simple.sh added 2010-08-06 by Rgerhards
# Updated 2025-04-16 for abstract sockets and multiple socket
# types.
echo ===============================================================================
echo \[uxsock_multiple_netns.sh\]: test for transmitting to another namespace
echo This test must be run with CAP_SYS_ADMIN capabilities [network namespace creation/change required]
if [ "$EUID" -ne 0 ]; then
    exit 77 # Not root, skip this test
fi

. ${srcdir:=.}/diag.sh init
require_netns_capable
check_command_available timeout

uname
if [ $(uname) != "Linux" ] ; then
    echo "This test requires Linux (AF_UNIX abstract addresses)"
    exit 77
fi

NS_PREFIX=$(basename ${RSYSLOG_DYNNAME})

SOCKET_NAMES=(
    "$RSYSLOG_DYNNAME-testbench-dgram-uxsock.0"
    "$RSYSLOG_DYNNAME-testbench-dgram-uxsock.DGRAM"
    "$RSYSLOG_DYNNAME-testbench-dgram-uxsock.STREAM"
    "$RSYSLOG_DYNNAME-testbench-dgram-uxsock.SEQPACKET"
)

# create the pipe and start a background process that copies data from
# it to the "regular" work file
generate_conf
for name in "${SOCKET_NAMES[@]}"; do
    ext=${name##*.}
    NS=${NS_PREFIX}.${ext}
    if [ ${ext} == "0" ]; then
        add_conf '

        $template outfmt,"%msg:F,58:2%\n"

        module(
            load = "../plugins/omuxsock/.libs/omuxsock"
            template = "outfmt"
            SocketName = "'${name}'"
            abstract = "1"
            NetworkNamespace="'${NS}'"
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
                NetworkNamespace="'${NS}'"
            )
            action(
                type = "omuxsock"
                SocketName = "'$name'"
                abstract = "1"
                SocketType = "'$ext'"
                NetworkNamespace="'${NS}.fail'"
            )
        '
    fi
done
add_conf '
}
'

BGPROCESS=()
for name in "${SOCKET_NAMES[@]}"; do
    ext=${name##*.}
    NS=${NS_PREFIX}.${ext}
    ip netns add "${NS}"
    ip netns exec "${NS}" ip link set dev lo up
    ip netns delete "${NS}.fail" > /dev/null 2>&1
    if [ "${ext}" == "0" ]; then
        type=""
    else
        type="-T${ext}"
    fi
    timeout 30s ip netns exec "${NS}" ./uxsockrcvr -a -s$name ${type} -o ${RSYSLOG_OUT_LOG}.${ext} -t 60 &
    PID=($!)
    BGPROCESS+=($PID)
    echo background uxsockrcvr ${name} process id is $PID
done

# now do the usual run
startup
# 10000 messages should be enough
injectmsg 0 10000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown

# wait for the cp process to finish, do pipe-specific cleanup
echo shutting down uxsockrcvr...
# TODO: we should do this more reliable in the long run! (message counter? timeout?)
for pid in ${BGPROCESS[@]}; do
    kill $pid
    wait $pid
done
echo background processes have terminated, continue test...

# Remove namespaces
for name in "${SOCKET_NAMES[@]}"; do
    ext=${name##*.}
    NS=${NS_PREFIX}.${ext}
    ip netns delete "${NS}" > /dev/null 2>&1
done

# and continue the usual checks
BASE=${RSYSLOG_OUT_LOG}
for name in "${SOCKET_NAMES[@]}"; do
    RSYSLOG_OUT_LOG=${BASE}.${name##*.}
    seq_check 0 9999
done
exit_test
