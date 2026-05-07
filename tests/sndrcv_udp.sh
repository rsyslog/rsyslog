#!/bin/bash
# This runs sends and receives messages via UDP to the standard
# ports. Note that with UDP we can always have message loss. While this is
# less likely in a local environment, we strongly limit the amount of data
# we send in the hope to not lose any messages. However, failure of this
# test does not necessarily mean that the code is wrong (but it is very likely!)
# added 2009-11-11 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[sndrcv_udp.sh\]: testing sending and receiving via udp
if [ "$EUID" -ne 0 ]; then
    exit 77 # Not root, skip this test
fi
export TCPFLOOD_EXTRA_OPTS="-b1 -W1"

# uncomment for debugging support:
. ${srcdir:=.}/diag.sh init
# start up the instances
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
export RSYSLOG_DEBUGLOG="log"
generate_conf
export PORT_RCVR_FILE="${RSYSLOG_DYNNAME}.imudp_port"
add_conf '
module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'$PORT_RCVR_FILE'")

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"'$RSYSLOG_OUT_LOG'" # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
assign_file_content PORT_RCVR "$PORT_RCVR_FILE"
export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
generate_conf 2
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
# this listener is for message generation by the test framework!
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

*.*	@127.0.0.1:'$PORT_RCVR'
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
tcpflood -m50 -i1
sleep 5 # make sure all data is received in input buffers
# shut down sender when everything is sent, receiver continues to run concurrently
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

# do the final check
seq_check 1 50

unset PORT_RCVR # TODO: move to exit_test()?
exit_test
