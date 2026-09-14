#!/bin/bash
# Cancel a qualified omfile FE callback after a real three-byte partial write
# while a BE sibling is waiting for its shared mutex. The preload handshake
# proves that exact waiter exists before shutdown; cancellation and reused
# markers prove the sibling later reaches the real stream with its own intact
# payload. Proper daemon termination is the cleanup oracle. External output is
# deliberately ambiguous: this test makes no exact-delivery claim after cancel.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin imdiag

export RSYSLOG_OMFILE_CANCEL_TARGET="$PWD/$RSYSLOG_DYNNAME.sink"
export RSYSLOG_OMFILE_CANCEL_EVENTS="$PWD/$RSYSLOG_DYNNAME.events"
export RSYSLOG_PRELOAD="./.libs/liblocal_queue_omfile_cancel_preload.so"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
    listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="16"
    queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
    queue.timeoutShutdown="1" queue.timeoutActionCompletion="1"
    queue.local.frontendSize="4" queue.local.maxFrontends="1")
template(name="cancelmsg" type="string" string="%msg%\n")
if ($msg contains "cancel") then
    action(type="omfile" file="'$RSYSLOG_OMFILE_CANCEL_TARGET'" template="cancelmsg"
        queue.type="Direct" asyncWriting="off" flushOnTXEnd="on")
'
startup
tcpflood -m1 -M'first-cancel'
wait_file_lines "$RSYSLOG_OMFILE_CANCEL_EVENTS" 1
content_check 'entered' "$RSYSLOG_OMFILE_CANCEL_EVENTS"
injectmsg_literal '<167>Mar  1 01:00:00 host tag sibling-after-cancel'
wait_file_lines "$RSYSLOG_OMFILE_CANCEL_EVENTS" 2
content_check 'waiting' "$RSYSLOG_OMFILE_CANCEL_EVENTS"
shutdown_immediate
wait_shutdown
content_check 'cancelled' "$RSYSLOG_OMFILE_CANCEL_EVENTS"
content_check 'reused' "$RSYSLOG_OMFILE_CANCEL_EVENTS"
exit_test
