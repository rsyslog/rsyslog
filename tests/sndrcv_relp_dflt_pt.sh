#!/bin/bash
# Verify that omrelp uses its compiled default port when no port parameter is
# configured. imrelp must explicitly bind the same default port because this is
# the behavior under test; success is the full ordered delivery sequence.
# added 2013-12-10 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
relp_port=$(./omrelp_dflt_port)
if [ $relp_port -lt 1024 ]; then
    if [ "$EUID" -ne 0 ]; then
	echo relp default port $relp_port is privileged
	echo need to be root to run this test - skipping
        exit 77
    fi
fi

# uncomment for debugging support:
# start up the instances
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp")
# then SENDER sends to this port (not tcpflood!)
input(type="imrelp" port="'$relp_port'")

$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
generate_conf 2
add_conf '
module(load="../plugins/omrelp/.libs/omrelp")

action(type="omrelp" target="127.0.0.1")
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
injectmsg2 1 50000

# shut down sender
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

seq_check 1 50000
exit_test
