#!/bin/bash
# added 2013-12-10 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=50000
########## receiver ##########
#export RSYSLOG_DEBUG="debug nostdout"
#export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"
generate_conf
export PORT_RCVR="$(get_free_port)"
add_conf '
module(load="../plugins/imrelp/.libs/imrelp")
input(type="imrelp" port="'$PORT_RCVR'")

$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
printf "#### RECEIVER STARTED\n\n"

########## sender ##########
#export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.sender.debuglog"
generate_conf 2
add_conf '
module(load="../plugins/omrelp/.libs/omrelp")

action(type="omrelp" name="omrelp" target="127.0.0.1" port="'$PORT_RCVR'")
' 2
startup 2
printf "#### SENDER STARTED\n\n"

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
injectmsg2 0 50000

shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

seq_check
exit_test
