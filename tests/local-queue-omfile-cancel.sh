#!/bin/bash
# Cancel a qualified omfile FE callback after a real three-byte partial write
# while a BE sibling is waiting for its shared mutex. The preload handshake
# proves that exact waiter exists before shutdown; cancellation and reused
# markers prove the sibling later reaches the real stream with its own intact
# payload. Proper daemon termination is the cleanup oracle. External output is
# deliberately ambiguous: this test makes no exact-delivery claim after cancel.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
# TSan's pthread/write interceptors cannot track cancellation unwinding through
# this uninstrumented preload shim: a standalone two-worker cleanup/unlock
# reproduction reports a double lock, while the same code without the shim is
# clean. Keep the real cancellation oracle in normal and ASan runs; do not mask
# lock reports in production code or disable TSan for other queue tests.
skip_TSAN "preload pthread/write interception misreports cancellation cleanup"
require_plugin imtcp
require_plugin imdiag

export RSYSLOG_OMFILE_CANCEL_TARGET="$PWD/$RSYSLOG_DYNNAME.sink"
export RSYSLOG_OMFILE_CANCEL_EVENTS="$PWD/$RSYSLOG_DYNNAME.events"
export RSYSLOG_PRELOAD="./.libs/liblocal_queue_omfile_cancel_preload.so"

generate_conf
# The local callback contract also applies to the harness diagnostic output.
localdiag_tmp="${TESTCONF_NM}.conf.localq"
{
    printf '%s\n' 'template(name="localdiag" type="string" string="%msg%\\n")'
    cat "${TESTCONF_NM}.conf"
} > "$localdiag_tmp" || error_exit $?
mv "$localdiag_tmp" "${TESTCONF_NM}.conf" || error_exit $?
sed -i.localq "s|file=\"./$RSYSLOG_DYNNAME.started\"|file=\"$PWD/$RSYSLOG_DYNNAME.started\" template=\"localdiag\"|" "${TESTCONF_NM}.conf" || error_exit $?
rm -f "${TESTCONF_NM}.conf.localq"
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
tcpflood -m1 -M'<167>Mar  1 01:00:00 host tag: first-cancel'
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
