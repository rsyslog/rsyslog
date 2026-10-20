#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the real classic Disk main queue's message serializer callback and
# readback on every imdiag testbench, including reduced Darwin ARM64 builds.
# The object method table stores a variadic pointer, but MsgSerialize requires
# two fixed arguments; an untyped call loses the stream argument on Darwin.
# Exact sequence output proves all injected messages survived disk round-trip.
# Harness queue draining and proper termination are the completion oracles;
# timeoutGuard bounds a broken callback or stalled queue without fixed sleeps.
. ${srcdir:=.}/diag.sh init
require_plugin imdiag
export NUMMESSAGES=100
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
global(workDirectory="'$PWD/$RSYSLOG_DYNNAME'.spool" processInternalMessages="off")
main_queue(queue.type="Disk" queue.filename="serializer-abi" queue.checkpointInterval="1")
template(name="serializerfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="serializerfmt")
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
seq_check
exit_test
