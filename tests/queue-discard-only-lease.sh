#!/bin/bash
# A one-record FixedArray queue reaches its discard watermark before the worker
# consumes it. The record is therefore a discard-only batch (nElem == 0,
# nElemDeq == 1). Empty shutdown must complete that batch and exit within ten
# seconds; no omfile output proves the record was discarded. The timeout is a
# deadlock oracle, while imdiag's queue-empty acknowledgement orders shutdown
# after dequeue.
#
# This file is part of rsyslog.
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(processInternalMessages="off")
main_queue(queue.type="FixedArray" queue.size="8"
    queue.workerThreads="1" queue.dequeueBatchSize="1"
    queue.discardMark="1" queue.discardSeverity="0")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown "" 10
if [ -e "$RSYSLOG_OUT_LOG" ]; then
    echo "discard-only batch unexpectedly reached omfile"
    error_exit 1
fi
exit_test
