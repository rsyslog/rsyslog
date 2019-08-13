#!/bin/bash
# This tests two rsyslog instances. Instance
# TWO sends data to instance ONE. A number of messages is injected into
# the instance 2 and we finally check if all those messages
# arrived at instance 1.
# added 2009-11-11 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
NUMMESSAGES=50000
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
# start up the instances
export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
# then SENDER sends to this port (not tcpflood!)
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.rcvr_port")

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"'$RSYSLOG_OUT_LOG'"
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
assign_rcvr_port $RSYSLOG_DYNNAME.rcvr_port

export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
generate_conf 2
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
# this listener is for message generation by the test framework!
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="omfwd" target="127.0.0.1" protocol="tcp" port="'$RCVR_PORT'")
' 2
startup 2
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
tcpflood -m$NUMMESSAGES -i1
wait_file_lines
# shut down sender when everything is sent, receiver continues to run concurrently
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

# do the final check
seq_check 1 $NUMMESSAGES

exit_test
