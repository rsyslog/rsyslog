#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify omudpspoof delivery to UDP destination port 2514 with source port
# 514. Send only 50 messages to limit UDP loss, and allow five seconds for
# receive buffers to drain. After both daemons exit, the receiver file
# must contain sequence 1 through 50.
# added 2009-11-11 by Rgerhards
# Developed by Adiscon GmbH for the rsyslog testbench.
# Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
echo ===============================================================================
echo \[sndrcv_omudpspoof_nonstdpt.sh\]: testing sending and receiving via omudp
echo This test must be run as root [raw socket access required]
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
add_conf '
$ModLoad ../plugins/imudp/.libs/imudp
# then SENDER sends to this port (not tcpflood!)
$UDPServerRun 2514

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"'$RSYSLOG_OUT_LOG'"
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
generate_conf 2
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
# this listener is for message generation by the test framework!
$InputTCPServerRun '$TCPFLOOD_PORT'

$ModLoad ../plugins/omudpspoof/.libs/omudpspoof
$template spoofaddr,"127.0.0.1"

#begin action definition
$ActionOMUDPSpoofSourceNameTemplate spoofaddr
$ActionOMUDPSpoofTargetHost 127.0.0.1
$ActionOMUDPSpoofTargetPort 2514
$ActionOMUDPSpoofSourcePortStart 514
$ActionOMUDPSpoofSourcePortEnd 514
*.*	:omudpspoof:
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
