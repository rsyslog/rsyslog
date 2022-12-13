#!/bin/bash
# This runs sends and receives messages via UDP to the non-standard port 2514
# Note that with UDP we can always have message loss. While this is
# less likely in a local environment, we strongly limit the amount of data
# we send in the hope to not lose any messages. However, failure of this
# test does not necessarily mean that the code is wrong (but it is very likely!)
# added 2009-11-11 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export TCPFLOOD_EXTRA_OPTS="-b1 -W1"
export NUMMESSAGES=50
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
$ModLoad ../plugins/imudp/.libs/imudp
# then SENDER sends to this port (not tcpflood!)
$UDPServerRun '$TCPFLOOD_PORT'

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"'$RSYSLOG_OUT_LOG'" # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
export RSYSLOG_DEBUGLOG="log2"
export PORT_RCVR="$TCPFLOOD_PORT"

#valgrind="valgrind"
generate_conf 2
add_conf '
*.*	@127.0.0.1:'$PORT_RCVR'
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
injectmsg2

# shut down sender when everything is sent, receiver continues to run concurrently
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

seq_check
exit_test
