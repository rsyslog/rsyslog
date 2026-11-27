#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the asynchronous flush timer with a partial final output
# buffer. Wait four seconds for the two-second flush interval before
# shutdown. After shutdown, require the complete sequence 0 through 35554.
#
# added 2010-03-09 by Rgerhards
# Developed by Adiscon GmbH for the rsyslog testbench.
# Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
echo ===============================================================================
echo TEST: \[asynwr_timeout.sh\]: test async file writing timeout writes
. ${srcdir:=.}/diag.sh init
export CI_SHUTDOWN_QUEUE_EMPTY_CHECKS=20 # this test is notoriously slow...
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
$OMFileFlushOnTXEnd off
$OMFileFlushInterval 2
$OMFileIOBufferSize 4k
$OMFileAsyncWriting on
:msg, contains, "msgnum:" ?dynfile;outfmt
'
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
startup
# send 35555 messages, make sure file size is not a multiple of
# 4K, the buffer size!
tcpflood -m 35555
sleep 4 # wait for output writer to write and empty buffer
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 35554
exit_test
