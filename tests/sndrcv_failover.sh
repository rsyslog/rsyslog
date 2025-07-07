#!/bin/bash
# This tests failover capabilities. Data is sent to a local port, where
# no process shall listen. Then it fails over to a second instance, then to
# a file. The second instance is started. So all data should be received
# there and none be logged to the file.
# This builds on the basic sndrcv.sh test, but adds a first, failing,
# location to the conf file.
# added 2011-06-20 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=50000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
export DEAD_PORT=4  # a port unassigned by IANA and very unlikely to be used
export RSYSLOG_DEBUGLOG="log"

# uncomment for debugging support:
# start up the instances
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
generate_conf
export PORT_RCVR="$(get_free_port)"
add_conf '
# then SENDER sends to this port (not tcpflood!)
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"'$RSYSLOG_OUT_LOG'" # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
export PORT_RCVR=$TCPFLOOD_PORT
generate_conf 2
add_conf '
*.*	@@127.0.0.1:'$DEAD_PORT' # this must be DEAD
$ActionExecOnlyWhenPreviousIsSuspended on
&	@@127.0.0.1:'$PORT_RCVR'
&	./'${RSYSLOG_DYNNAME}'.empty
$ActionExecOnlyWhenPreviousIsSuspended off
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

# do the final check
seq_check

ls -l ${RSYSLOG_DYNNAME}.empty
if [[ -s ${RSYSLOG_DYNNAME}.empty ]] ; then
  echo "FAIL: ${RSYSLOG_DYNNAME}.empty has data. Failover handling failed. Data is written"
  echo "      even though the previous action (in a failover chain!) properly"
  echo "      worked."
  error_exit 1
else
  echo "${RSYSLOG_DYNNAME}.empty is empty - OK"
fi ;
exit_test
